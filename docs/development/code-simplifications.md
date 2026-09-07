# Code simplification follow-ups

Investigation base: `6af852a6d0dffeb6c812c86bc37c04e10182b804`. The agreed work proceeds in separate review slices.

| Slice | Change | Status |
| --- | --- | --- |
| S01 | Share the Linux and Windows native-test launchers | Implemented below |
| S02 | Consolidate Windows counter-widening state | Planned |
| S03 | Remove redundant resets during Linux event construction | Planned; preserve explicit reset semantics and check fresh groups |
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
