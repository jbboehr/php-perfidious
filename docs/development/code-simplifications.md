# Code simplification follow-ups

Investigation base: `6af852a6d0dffeb6c812c86bc37c04e10182b804`. The agreed work proceeds in separate review slices.

| Slice | Change | Status |
| --- | --- | --- |
| S01 | Share the Linux and Windows native-test launchers | Implemented below |
| S02 | Remove redundant Windows counter initialization flags | Implemented below |
| S03 | Remove redundant resets during Linux event construction | Implemented below |
| S04 | Use the main Nixpkgs pin for PHP 8.4 | Implemented below |

## S01: shared native-test launcher

[The helper](../../tests/native-harness.inc) replaces repeated executable discovery, subprocess handling, compilation,
and cleanup in the [Linux shim](../../tests/sampler/linux-perf-shim.phpt),
[Linux live fixture](../../tests/sampler/linux-perf-live.phpt), and
[Windows shim](../../tests/windows/sampler-shim.phpt). The extraction removes 143 net lines from test support code.

Each PHPT retains its platform and byte-order restrictions, source file, extra compiler flags, diagnostic label, and
expected output. The C fixtures and their assertions are unchanged. Tools beside the tested PHP executable retain
precedence over PATH. Child commands clear `LD_PRELOAD` and `DYLD_INSERT_LIBRARIES`, preserve the rest of the environment,
and merge stderr into stdout. Temporary binaries now share the `perfidious-native-harness-` prefix and are removed
at PHP shutdown, including on command failure.

Darwin's probe launcher and the FPM runners remain separate: their stream handling, tool selection, or sanitizer
environment differs from these three launchers.

### Verification

Linux x86-64, PHP 8.1.34 debug, 2026-09-07:

- All three affected PHPTs passed before and after extraction, with no skips.
- A temporary characterization runner passed 24 cases before and after extraction: disabled process execution,
  missing compiler/configuration tools, sibling-tool precedence, PATH fallback, and configuration/compiler/binary
  failures. It checked both output streams, large compiler diagnostics, preload removal, retained sanitizer settings,
  and temporary-binary cleanup.
- Temporary helper mutations that retained preload variables, reversed tool precedence, ignored compiler failure,
  or omitted cleanup each failed those checks for the expected reason.
- The full PHPT suite passed 83 tests with 34 skipped, no warnings, and no failures.
- Composer validation, aggregate-stub freshness, PHP_CodeSniffer, and all five documented PHPStan commands passed.
  The new helper also passed PHP syntax and explicit PHP_CodeSniffer checks.

The characterization and mutation runners were one-off experiments. No production source changed. Native
Windows/macOS, other PHP configurations, VM, and sanitizer suites were not rerun for this extraction. The Windows
fixture used substitute native calls on Linux; it does not establish native Windows execution.

## S02: Windows counter initialization flags

Review base: `71a9f0ff7a59d2b6f97c1af7d088a95cf051ea38`.

[The Windows sampler](../../src/windows/sampler.c) keeps the explicit previous native count and 64-bit wrap base for
each counter. The two initialization flags are removed: `ecalloc()` initializes the previous counts to zero, and an
unsigned native count cannot be less than zero on its first observation. Six fields become four. Wrap detection and
overflow checks retain their existing comparisons and additions.

The counters remain independent. Page-fault observations are retained before a later process-cycle query can fail;
context-switch state is committed at the end of the read. Exception classes and messages are unchanged. More than one
wrap between native observations remains unobservable.

### Verification

Linux x86-64, PHP 8.1.34 debug, 2026-09-07:

- The Windows shim PHPT passed with assertions for zero and nonzero first readings, repeated readings, the last
  representable wrap, exact `UINT64_MAX` readings, overflow rejection and recovery, and successive wraps observed
  during failed cycle queries. Both counters reject a repeated overflowing observation without changing their state.
- The native fixture compiled with GCC and `-Wall -Wextra -Werror` and passed. A separate ASan/UBSan build passed
  with leak detection enabled and no sanitizer reports.
- Four temporary mutations each failed a relevant assertion: treating a repeated value as a wrap, updating either
  counter's previous reading before rejecting overflow, and discarding observations from failed reads.
- The full PHPT suite passed 83 tests with 34 skipped, no warnings, and no failures. Composer validation, aggregate-stub
  freshness, PHP_CodeSniffer, and all five documented PHPStan commands passed.
- Configured lint hooks, C formatting, local documentation links, and `git diff --check` passed.

These native checks use the real sampler with substitute Windows calls on Linux. Native Windows execution and SDK/ABI
compatibility remain unverified. Other PHP configurations, native macOS, VM, and full-extension sanitizer suites were
not rerun for this slice.

## S03: Linux event construction resets

Review base: `c3c232999702c1388c98096d81d89aa3ed038702`.

[Handle construction](../../src/handle.c) no longer resets each newly opened event. Linux
[zero-initializes event storage](https://github.com/torvalds/linux/blob/v6.12/kernel/events/core.c#L12146), and
[group members cannot count until their leader is enabled](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).
The handle opens its dummy leader disabled, so the initial resets have no counts to clear. Removing them saves one
ioctl and its failure path per descriptor: `N + 1` calls for `N` requested events.

Explicit `Handle::reset()` and request-lifecycle resets retain their group reset, timing baselines, and enabled-state
handling. Opening and event-ID lookup failures retain their existing cleanup paths.

### Verification

Linux x86-64, PHP 8.1.34 debug, 2026-09-07:

- The new [initial-state test](../../tests/handle/zero-before-enable.phpt) passed before and after removal. It checks
  empty, single-event, and multi-event groups before enabling, including reopening after use. Counts and both timing
  totals remain zero during intervening CPU work. A temporary mutation that opened the leader enabled failed its
  assertions; the original source was restored before the refactor.
- Six focused PHPTs passed before and after removal: initial state, zero counts after reset, enabled-state preservation,
  reset timing/scaling, partial-construction cleanup, and explicit descriptor errors.
- A one-off experiment passed 70 group lifecycles against each module: ten each for empty groups, task clock, CPU clock,
  page faults, instructions, cycles, and mixed hardware/software events. It checked initial zeros, counting after enable,
  explicit resets while disabled and enabled, and cumulative timing. Strace recorded 160 event opens in each run;
  reset calls fell from 300 to 140, leaving exactly the two explicit group resets per lifecycle.
- The rebuilt module passed the full suite: 84 passed, 34 skipped, no warnings, and no failures. Composer validation,
  aggregate-stub freshness, PHP_CodeSniffer, and all five documented PHPStan commands passed. The new PHPT body passed
  syntax and formatting checks, excluding the side-effect rule for its combined helper and script body.
- Configured lint hooks, local documentation links, and `git diff --check` passed. Whole-file C formatting differences
  predate this slice; comparison with the same formatter configuration found no added diagnostics.

These runs exercised real user-only perf events on the local kernel. Other kernels, architectures, PHP configurations,
VM, native Windows/macOS, Valgrind, and sanitizer suites were not rerun for this slice.

## S04: PHP 8.4 uses the main Nixpkgs pin

Review base: `abe7b35`.

[The flake](../../flake.nix) now takes PHP 8.4 from the existing `nixos-26.05` pin (`21ea275a7c46`), alongside PHP 8.2,
8.3, and 8.5. The dedicated `nixpkgs-unstable` input (`0954f7ee2f6b`) and its lock entry are removed. All retained lock
entries are unchanged. PHP 8.1 still uses `nix-phps` and its own transitive Nixpkgs pin.

PHP remains at 8.4.23, with the same 53 loaded extensions. The runtime's dependency closure changes:

| Runtime component | Before | After |
| --- | --- | --- |
| PCRE | 10.47 | 10.46 |
| SQLite | 3.53.3 | 3.51.2 |
| ICU | 73.2 | 73.2 |
| Closure paths | 121 | 121 |
| Unpacked closure size | 249.1 MiB | 248.6 MiB |

Other transitive libraries also change with the pin. Both PHP runtimes load without startup warnings. One-off checks
passed Unicode regular expressions and UTF-8 database round trips through both SQLite3 and PDO SQLite before and after
the switch. These checks establish basic runtime compatibility, not equivalence of every dependency feature.

### Verification

Linux x86-64, PHP 8.4.23 NTS, 2026-09-07:

- `nix flake check --no-build --all-systems` passed. Comparing evaluated derivation paths on x86-64 and AArch64 showed
  changes only to the four PHP 8.4 package/check/shell variants and the lint check. Output names and
  the generated CI matrix are unchanged.
- A fresh baseline `checks.x86_64-linux.php84-gcc` build passed 69 tests with 49 skips in both its ordinary and
  Valgrind 3.26.0 suites. The replacement GCC, Clang, and GCC coverage checks each passed with the same counts.
  GCC debug coverage passed 79 tests with 39 skips in both suites. All runs reported zero failed or leaked tests.
- All four installed modules loaded with the expected debug settings. Both coverage checks produced LCOV data and HTML
  reports. Installed-module smoke runs used separate `GCOV_PREFIX` directories to keep release and debug profile data
  from sharing their compiled-in paths.
- Aggregate-stub freshness, PHP_CodeSniffer, and all five documented PHPStan commands passed under the replacement PHP.
  Composer validation, configured lint hooks, local documentation links, and `git diff --check` passed.

AArch64 was evaluated only. Suite skips covered native Windows/macOS, restricted perf events, PID permissions, ZTS,
debug-only cases in release builds, and FPM fixtures requiring Python 3. Native non-Linux, VM, and sanitizer runs were
not repeated for this pin consolidation.
