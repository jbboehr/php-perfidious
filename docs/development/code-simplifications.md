# Code simplification follow-ups

Investigation base: `6af852a6d0dffeb6c812c86bc37c04e10182b804`. The agreed work proceeds in separate review slices.

| Slice | Change | Status |
| --- | --- | --- |
| S01 | Share the Linux and Windows native-test launchers | Implemented below |
| S02 | Remove redundant Windows counter initialization flags | Implemented below |
| S03 | Remove redundant resets during Linux event construction | Implemented below |
| S04 | Use the main Nixpkgs pin for PHP 8.4 | Planned; validate the changed dependency closure |

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
