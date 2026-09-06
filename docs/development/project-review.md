# Project review and experimental verification

Reviewed revision: `4ec048b2cbb38ff1b7af49642fdaf1732a9d6706`.
Follow-up experiments: 2026-09-05 UTC, Linux x86-64, PHP 8.1.34, debug extension build.

The initial review did **not** experimentally verify every finding. It combined source inspection, native API
documentation, ordinary tests, and three focused experiments. Follow-up checks covered FPM attribution, counter reset,
descriptor flags, referenced strings, PMU metadata, and exception behavior. A further verification pass exercised the
Darwin sampler through its Linux shim, the actual counter-scaling output with controlled readings, the proposed portable
configure correction, and CI log selection with a populated fixture.

This report records what was observed, what follows from source or API contracts, and what remains uncertain. A passing
test suite does not establish that an individual finding is absent. The initial review did not change production code;
subsequent implementation decisions are recorded below.

## Follow-up: remove cumulative global counters

The global-counter API and its configuration have been removed following review.
The feature accumulated totals for an individual worker and offered limited value without a worker telemetry consumer.
Explicitly owned handles and the automatic request handle remain available. The removal is documented as a breaking
change in the changelog and includes migration guidance in the README.

The global-specific tests were removed with the feature. Shared ownership, invalid-configuration, and read-error
coverage remains on the request handle; the phpinfo scaling test now also uses that handle. Before changing the native
implementation, the updated API declaration and phpinfo expectations produced three expected test failures. After the
removal, all nine focused tests passed. The independent test review then added
[a regression test](../../tests/global-counter-removal.phpt) covering inert legacy configuration, the removed API and
phpinfo output, and retained request/owned handles. It failed against the pre-removal module and passed against the
changed module. The final full Linux PHP 8.1.34 suite reported 71 passed, 20 skipped, zero failures.
The updated FPM fixture also handled 13 requests in one local worker, including a deferred lifecycle error and recovery.

All ten focused tests, including the new regression, passed under Valgrind with Zend allocation disabled, with zero
reported test leaks.
Composer validation, generated-stub freshness, PHP syntax, PHP_CodeSniffer, PHPStan and all four declaration-analysis
configurations passed. Markdown and Nix formatting passed, and the debug FPM VM derivation evaluated successfully.
A runtime check with legacy global settings confirmed that only the request settings remain registered, the global
function and phpinfo table are absent, and the request counter remains readable.

The full NixOS VM and native Windows/macOS builds were not run for this removal. An optional clang-format check reports
existing macro/declaration formatting violations in `src/functions.c` and `php_perfidious.h`; comparison against the
base revision found no newly introduced violations.

**Removal review verdict: PASS_WITH_RESIDUAL_RISK.** The independent correctness review found no defects in scope, and
the independent test review added the regression above without finding a production defect. The remaining verification
limits are the native platforms and full VM execution noted above. The final full suite and focused Valgrind run were
repeated after those reviews.

R01 is addressed in the next follow-up. R03, R04, R06, and R11 remain applicable to retained code.
Their fixes will be reviewed separately.

## Follow-up: R01 request-counter ownership

Implementation review base: `81e73db` (after removing global counters).

The request handle now stays null through GINIT and MINIT. RINIT opens it in the process and thread serving the
request, then retains it for subsequent requests. The existing reset, enable, and disable operations still delimit
requests. This avoids attributing worker measurements to the FPM master.

Moving the open alone is insufficient when opcache preloading runs a startup request in the master.
[PHP 8.1's preload implementation](https://github.com/php/php-src/blob/PHP-8.1/ext/opcache/ZendAccelerator.c#L4311)
calls request startup and shutdown; non-root preloading runs in the initializing process.
RINIT therefore records the opening process ID and replaces an inherited handle when the process changes.
It closes the inherited descriptors without resetting or disabling the parent's event group.

Cleanup now belongs to GSHUTDOWN, using the supplied module-globals pointer. This covers globals destroyed for an
individual ZTS thread as well as ordinary module teardown. Native cleanup does not raise PHP diagnostics or depend
on accessing another thread's globals. The low-level opening path similarly returns an error record, allowing
RINIT to defer errors without creating a PHP exception during startup.

An invalid metric now produces a catchable `PmuEventNotFoundException` from `request_handle()`, instead of a startup
warning followed by a permanently disabled request counter. Perf access failures produce `IOException`.
The first pending initialization or lifecycle error is consumed once; later calls return null if preparation failed.
An absent handle is retried on the next request. The record contains a fixed-size message and native error code,
so no request-allocated exception or string is retained between requests.

```php
// With perfidious.request.enable=1 and an invalid perfidious.request.metrics value:
try {
    $handle = Perfidious\request_handle();
} catch (Perfidious\PmuEventNotFoundException | Perfidious\IOException $error) {
    error_log($error->getMessage());
    $handle = null;
}
```

### R01 experimental evidence

The new [FPM regression](../../tests/request-handle/fpm-worker.phpt) failed against the pre-fix module:
both workers returned zero request-counter deltas, while fresh counters measured approximately 100 ms in each of
four requests. The [preload regression](../../tests/request-handle/fpm-preload.phpt) failed the same way.
The [initialization-error regression](../../tests/request-handle/initialization-error.phpt) also failed before the fix,
because the error appeared during startup rather than at the API call.

All three pass after the change. A direct two-worker run measured request/fresh deltas of
100,005,273 / 100,007,373 ns and 100,013,353 / 100,015,003 ns on first requests. With master-process preloading,
the first-request pairs were 100,011,753 / 100,010,123 ns and 100,003,833 / 100,002,413 ns.
The tests also check both workers across a second request, reset values, and debug opening counts to detect
unnecessary reopening. A fresh counter must advance; a zero request counter is never accepted as a reason to skip.

To repeat the FPM tests, install the paired `php-fpm` beside the tested PHP interpreter (`PHP_BINARY`), put `python3`
on PATH, and run
`make test TESTS='tests/request-handle/fpm-worker.phpt tests/request-handle/fpm-preload.phpt'`.
The harness resolves the loaded extension from `/proc/self/maps` so it tests that artifact rather than assuming
the workspace's module is the one being tested. It skips when the paired FPM executable is unavailable.
When opcache is outside PHP's extension directory, set `PERFIDIOUS_TEST_OPCACHE` to its shared-module path.
The preload case requires a non-root user to exercise preloading in the master rather than PHP's privileged
preload-child path. These tests use temporary local Unix sockets and terminate their FPM processes afterward.

The existing single-worker FPM fixture also completed 13 requests: ten ordinary requests, a deliberately injected
shutdown error, delivery of that deferred error, and recovery in the same worker.
The final full Linux PHP 8.1.34 debug suite passed with 75 tests passing, 20 skipped, and zero failures.
Thirteen focused CLI tests also passed under Valgrind with Zend allocation disabled, with zero reported leaks.
These cover successful and failed opening, owned and borrowed handles, event-name lifetime, and request shutdown.
The FPM subprocesses in the attribution tests were not run under Valgrind.

The PHP 8.5 ZTS Nix check built successfully. A direct run of 42 selected tests against its PHP 8.5.8 ZTS release
module reported 36 passed, six debug-only skips, and zero failures. These include request initialization, invalid
configuration, phpinfo, API declarations, and the owned-handle suite. This exercises a ZTS CLI process, not concurrent
request threads or a threaded SAPI; thread creation/destruction remains a native integration-test gap.

Composer validation, generated-stub freshness, PHP syntax, PHP_CodeSniffer, PHPStan, all four declaration-analysis
configurations, and Markdown checks passed. Native Windows/macOS and the full NixOS FPM VM were not run for R01.

**R01 review verdict: PASS_WITH_RESIDUAL_RISK.** The independent correctness review found no defects in scope.
The independent test review added a
[multi-request initialization-error test](../../tests/request-handle/fpm-initialization-error.phpt), which verifies
the exception class, code `-4`, message, consume-once behavior, and a new opening attempt on the second request.
It also corrected the FPM harness's artifact selection: external-artifact runs previously could exercise the
workspace module and PATH's FPM instead of the intended build. The corrected helper selects the loaded module
and paired FPM, and correctly skips the supplied ZTS artifact's FPM tests because that distribution has no FPM.
No production defect was demonstrated by the test review. The full suite and focused Valgrind run were repeated
after both reviews; concurrent ZTS thread teardown and native platform/VM coverage remain the limits noted above.

### R01 review follow-up

The `tmp.md` handoff and Codex's own independent review were evaluated against the pending changes on `984beb4`.
The separate review summary reporting no actionable regressions was also considered; all four handoff findings
were evaluated individually.

| Finding | Decision and evidence |
| --- | --- |
| Clear an unconsumed initialization error after successful retry | Declined the proposed semantic change. Retaining the first pending error until the API is called is the chosen diagnostic contract. A three-request FPM experiment verified that a failed open can remain unobserved, the next request can open successfully and deliver that pending error once, and subsequent calls receive the usable handle. The third request has no pending error and reuses the handle. The README and stubs now state this explicitly. |
| Disabled pools retain inherited preload descriptors | Fixed. A real non-root FPM master opened two perf descriptors during preloading; a pool configured with request counting disabled retained both across two requests. Moving the enable check below PID-change cleanup makes both worker requests report zero perf descriptors, without opening replacement counters. The master still owns its descriptors. |
| README's nullsafe-only example omits exception handling | Improved. The example now catches initialization and I/O exceptions. The configuration table links to that end-user example rather than to the development report. |
| Preload SKIPIF examines real UID while the harness uses effective UID | Fixed. Both use effective UID. Controlled status-line inputs showed the old expression skipped real-root/effective-nonroot and admitted real-nonroot/effective-root; the corrected expression makes the opposite decisions. Actual mixed-UID processes were not launched. |

The independent review also verified the handoff's note about Python optimization: with `PYTHONOPTIMIZE=1`, the
new disabled-pool regression incorrectly passed against the leaking implementation because Python removed its
assertions. The harness now uses explicit checks that raise on failure. With optimization still enabled, the same
test correctly failed before the native fix and passed afterward. All eleven request-handle tests pass with
optimization enabled.

The pending-error experiment is retained as
[a characterization test](../../tests/request-handle/fpm-unconsumed-error.phpt), and descriptor cleanup is protected by
[the disabled-pool regression](../../tests/request-handle/fpm-preload-disabled.phpt). No production fault-injection
API was needed. Nameless-UID container support and special handling for permanently invalid configuration remain
optional and were not added; they are not required for the current supported test setup or retry policy.

The full Linux suite after these changes reported 77 passed, 20 skipped, and zero failures. Composer validation,
stub generation checks, PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations passed.
The final full-suite repetition produced the same result. Thirteen focused Valgrind tests also passed with Zend
allocation disabled and no reported leaks. PHP syntax, Python parsing, Markdown checks, and the final diff check
passed. The focused correctness review found no additional defects. Concurrent ZTS teardown, native Windows/macOS,
and full VM integration remain unverified for this follow-up.

The independent test pass completed six focused tests normally and five FPM lifecycle tests with Python
optimization enabled. Its optional syscall-level tracing experiment was not completed; the absence of reset/disable
calls on inherited-handle cleanup was checked in the source. **Follow-up verdict: PASS_WITH_RESIDUAL_RISK**, with
the execution limits listed above. No further production changes were required by that review pass.

## Follow-up: R02 Darwin process CPU-time units

Implementation review base: `debb92e` (after R01).

The process resource-usage API and process sampler now use the existing checked Mach-to-nanosecond conversion.
Each user/system field is converted before summing or checking the PHP integer range. Hardware counter values
remain unscaled. MINIT initializes the timebase even when `thread_selfcounts` is absent. If timebase initialization
fails, process CPU-time calls throw `IOException`; the thread fallback and unrelated sampler metrics remain usable.

The upstream source trace was checked again for this slice: Apple's
[task accounting](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/task.c#L6391) supplies Mach units,
[rusage assignment](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c#L1196) copies them,
and the [kernel return path](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c#L3271)
and [libproc wrapper](https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/libproc/libproc.c#L129)
return them without conversion. This confirms the source-level basis for R02; native macOS execution remains a
separate verification requirement.

### R02 experimental evidence

The new [CPU-time regression](../../tests/darwin/cpu-time-shim.phpt) builds the actual Darwin extension sources as a
shared module on Linux, substituting only native calls. It runs the public PHP resource-usage and sampler APIs,
including module initialization and the existing conversion helper. No production test hooks were added.

Before the fix, the test failed on both process properties and the sampler: for user/system readings of
18,000,000 / 6,000,000 ticks with a `125/3` timebase, they exposed 18,000,000 / 6,000,000 and 24,000,000 respectively.
Afterward, they expose 750,000,000 / 250,000,000 ns and 1,000,000,000 ns, matching independent arithmetic.
The same test covers `1/1`, `2/3`, and `3/2` ratios, per-field rounding, multiplication that would overflow before division,
conversion overflow in either field, PHP integer overflow after scaling, and total CPU-time overflow after conversion.
It also verifies absent/present thread SPI, invalid ratios, failed timebase initialization, unchanged hardware counts,
and the seconds/microseconds thread fallback.

Run it with `make test TESTS='tests/darwin/cpu-time-shim.phpt tests/darwin/sampler-probe-shim.phpt'`.
The new test requires a 64-bit Linux PHP without a built-in perfidious extension, matching PHP development headers
from `php-config`, and a C compiler. A statically linked backend cannot be replaced by the isolated test module.
The existing sampler-only harness retains a `1/1` conversion substitute; the new shared-module test exercises the
real conversion and both PHP APIs.

A separate [native oracle test](../../tests/darwin/process-cpu-time-native-oracle.phpt) brackets both process APIs
with PHP's `getrusage()` readings, converting their seconds/microseconds independently. It was added for native
macOS validation and skipped on this Linux host; it has not been observed passing or failing on macOS.

The final Linux PHP 8.1.34 debug run reported 78 passed, 21 skipped, and zero failures. Both compiled Darwin
tests passed. Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration
analysis configurations passed. PHP syntax, Markdown, and diff checks also passed.

The isolated Darwin module was also exercised under Valgrind using the direct PHP executable and
`USE_ZEND_ALLOC=0`, with `--leak-check=full --errors-for-leak-kinds=definite --error-exitcode=99`.
The `125/3`, `1/1`, and `2/3` fixtures each reported zero errors and zero bytes in use at exit.
These runs exercised the substitute native calls on Linux, not macOS APIs.

**R02 review verdict: PASS_WITH_RESIDUAL_RISK.** The independent correctness review found no introduced defect.
Both reviews considered changing the thread fallback for a successful but invalid timebase with SPI present;
comparison with the base revision showed that this would change pre-existing behavior outside R02, so the proposal
and its new fallback expectations were withdrawn.

The independent test review corrected the large-value fixture to use 12,000,000,000,000,000,000 ticks at `2/3`,
whose intermediate product exceeds `uint64_t` while its final 8,000,000,000,000,000,000 ns value fits a PHP integer.
It also added a `3/2` fixture where the whole conversion term fits exactly but the fractional term overflows the sum.
In temporary copies, replacing the checked conversion with direct multiplication caused both public APIs' large-value
assertions to fail; removing the final addition check caused both APIs' expected overflow exceptions to disappear.
The focused and full suites were repeated after this test hardening. Native macOS compilation, real Apple API calls,
and the native `getrusage()` oracle remain unverified.

### R02 review follow-up

The `tmp.md` handoff and the separate review summary reporting no actionable regressions were considered alongside
Codex's own independent review of the staged tests, unstaged production changes, and surrounding conversion,
initialization, error, and sampler paths. No further production or test changes were warranted.

- **Native oracle tolerance:** retained the 2 ms margin pending a native run. The handoff identified a possible
  portability risk, not an observed failure. Current XNU's
  [`calcru()`](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c#L1641) converts Mach
  accounting to seconds/microseconds; that path does not establish the handoff's claimed fixed 10 ms resolution.
  This source check does not validate the margin across macOS versions. On a native failure, inspect the actual
  bounds and deltas before changing either the tolerance or the conversion.
- **Platform timebase labels:** corrected the handoff's assumptions in this record. XNU's
  [x86 implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/i386/rtclock.c#L384) explicitly
  supplies `1/1`, while its
  [ARM implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/arm/rtclock.c#L99) derives the
  ratio from the platform frequency. A native conversion check must establish the actual machine's ratio;
  do not assume Apple Silicon uses `1/1` or Intel uses `125/3` as the handoff did.
- **PHP development headers:** confirmed the harness uses `php-config` from PATH. Matching headers remain a
  documented prerequisite; a mismatch produces a visible build/load failure. No test-selection change was needed.
- **Arithmetic and thread behavior:** confirmed that `remainder * numer` fits `uint64_t` because both factors are
  bounded by the 32-bit timebase fields. The successful-but-invalid timebase with SPI present retains its
  pre-existing thread error behavior, as already evaluated above. Neither note warrants another production change.

Fresh verification rebuilt the Linux module and passed both compiled Darwin tests; the native oracle was skipped.
The full suite reported 78 passed, 21 skipped, and zero failures with `PERFIDIOUS_TEST_OPCACHE` set to the available
opcache module. The external summary's 76 passed / 23 skipped result is recorded as its separate run.
Composer validation, stub freshness, PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations,
PHP syntax, Markdown, and staged/unstaged diff checks passed. Native macOS compilation and execution remain
unverified. This follow-up changes the development report only.

## Follow-up: R03 reset timing and request scaling

Implementation review base: `345c84b`.

Each handle now records the kernel's enabled/running totals at its latest successful reset. `phpinfo()` subtracts
those baselines before calculating the scaled count and running percentage. The public `read()` timing fields,
`rawStream()`, and native raw-read API retain their kernel-lifetime semantics. Counts still start at zero after reset.
The README and declarations now explain this distinction for callers doing their own scaling.

Reset briefly disables an active group, reads its timing totals, resets every count, and restores the prior enabled
state. Keeping the group disabled aligns the timing snapshot with the count reset. A failed read or reset retains
the previous baseline; a failed resume leaves the handle disabled and returns an error. The lifecycle helper uses
native allocation and errno results without PHP diagnostics, preserving deferred request-startup/shutdown errors.
Request shutdown now disables before resetting, avoiding an unnecessary resume during shutdown.

The active-reset regression exposed a related group-control problem that blocked this implementation. On the local
Linux 7.1.5-xanmod1 kernel, disabling and enabling each sibling through `PERF_IOC_FLAG_GROUP` could leave software
counts stopped until another scheduling event. A 10 ms PHP workload after an ordinary disable/enable cycle returned
only 1,720 ns; after the initial reset implementation it returned zero. A standalone native group experiment also
undercounted with that flag. Enabling/disabling the leader with argument zero counted the full interval. This follows
the documented [group-leader control semantics](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).
The helper now controls the group's enabled state through its leader, keeping siblings eligible to run together.
The reset ioctl still uses the group flag because it must clear every member's count.

### R03 experimental evidence

Before implementation, a real software task-clock reading was 49,995,196 ns for the count and both timing fields.
After reset, the count was zero and both public timing fields remained 49,995,196 ns.

The new [scaling regression](../../tests/info-reset-scaling.phpt) performs two real intervals and resets, checks that
counts clear while public timings stay cumulative, then supplies a controlled next interval using the existing
synthetic-read fixture. Its count is 50, enabled duration 100, and running duration 50. Before the fix, the actual
phpinfo row was `50 => 50 => 99%`; afterward it is `50 => 100 => 50%`.
The test uses the latest real baseline and preserves the complete group layout and event IDs.

The [active-reset test](../../tests/handle/reset-enabled.phpt) failed with a zero count after the first implementation.
After correcting leader control, it confirms counting resumes after an active reset. The independent test review
strengthened it to check two software-event members, all-member reset, and unchanged counts/timing throughout a
workload after a disabled reset. No sleep was added to hide the scheduling behavior.

Eight local `strace` fault-injection experiments exercised the new reset path. Each first traced a marked reset call,
located the target read/ioctl's syscall ordinal, then repeated the same program with `-e inject=SYSCALL:error=EIO:when=N`
or `-e inject=read:retval=0:when=N`. The trace confirmed injection occurred inside that reset call.

| Initial state | Injected failure | Observed result |
| --- | --- | --- |
| Disabled | Read error, empty read, or reset ioctl error | `IOException` with EIO; counts and previous scaling baseline retained |
| Enabled | Disable, read, empty read, or reset ioctl error | `IOException` with EIO; counting remains enabled or resumes |
| Enabled | Resume ioctl error | `IOException` with EIO; counting remains disabled |

The retained-baseline checks supplied a subsequent interval and verified the phpinfo result `50 => 100 => 50%`.
These fault experiments are supplementary local checks, not permanent PHPT coverage. An independent test pass also
injected disable/read/reset/resume errors and checked recovery with two members, plus prior-baseline retention.
A direct `rawStream()` experiment decoded the group record after reset: its count was zero, and both timing totals
equaled the public pre-reset reading.

### R03 review and final verification

**Verdict: PASS_WITH_RESIDUAL_RISK.** The independent correctness review found no actionable defects. The independent
test review added the multi-member state checks above without finding a production defect. Final verification after
that test change passed:

- `make -j2` and `make test TESTS='tests/handle/reset-enabled.phpt tests/info-reset-scaling.phpt'`: both focused tests.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test`, with `PERFIDIOUS_TEST_OPCACHE` set to the available opcache module:
  80 passed, 21 skipped, zero failures, including the local FPM preload tests.
- Seven focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'`: seven passed, zero reported leaks.
  These covered reset, enabled/disabled state, scaling, broken descriptors, and deferred lifecycle errors.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan and all four declaration-analysis
  configurations, changed PHP syntax, Markdown, and final diff checks.

Native allocation failure and persistent FPM startup/shutdown syscall-fault sequences were not injected. Changing
hardware multiplex ratios were not induced; the exact scaling oracle uses synthetic timing increments after a real
reset. Native Windows/macOS, concurrent ZTS request execution, and the full NixOS VM were not run for this slice.

### R03 review follow-up

The `tmp.md` handoff and the separate review summary were read and considered alongside Codex's own independent
review of the complete uncommitted diff, reset/error transitions, request lifecycle, raw reads, group opening and
control, scaling, and tests. No further production or test change was warranted.

The handoff's optional shutdown-retry observation is accurate under a transient injected failure. A local CLI
experiment marked the end of user code, located the first shutdown-disable syscall, and injected one EIO with
`strace`. The resulting native sequence was:

```text
DISABLE leader: EIO (injected)
DISABLE leader: success
RESET group:   success
ENABLE leader: success
```

The mitigation is deferred, rather than treating the observation as incorrect. Remaining enabled after a failed
shutdown disable was already possible at the review base, and the next successful RINIT reset still establishes
fresh count/timing boundaries. No naturally occurring fail-once/succeed-on-retry trigger or incorrect next-request
measurement was established. The upstream
[perf ioctl path](https://github.com/torvalds/linux/blob/master/kernel/events/core.c#L6205) checks security authorization
and event revocation, then applies the disable operation; that source check does not establish a transient failure.
This remains a possible error-recovery improvement, with injected CLI evidence rather than an observed FPM incident.

The other handoff notes do not require fixes: the three-column read-error header predates R03; the overflow fixture's
zero baseline matches its first CLI request on a never-enabled group; allocation-failure and kernel-portability
limits remain documented. Leader-only enable/disable is retained.

Fresh verification rebuilt the module, passed five focused tests, and reported 80 passed / 21 skipped in the full
suite with the opcache module configured, including both FPM preload cases. A separate real mixed
hardware-instruction/software-task-clock group resumed both members after active reset, cleared both counts on
disabled reset, and kept public timing totals frozen during subsequent work. Composer validation, stub freshness,
PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations, PHP syntax, Markdown, and final diff
checks passed. The external review's 78 suite tests plus two separately rerun preload tests are its own verification
record. Concurrent ZTS execution, the skipped platform tests, and full VM execution remain unverified.

## Follow-up: R04 factory resource ownership

Implementation review base: `6224216`.

Linux `Perfidious\open()`, common `Sampler::open()`, and Windows
`enable_current_thread_profiling()` now allocate an empty PHP cleanup owner before acquiring native resources.
The sampler also allocates its identity first and attaches backend state before its initial read. Ordinary open/read
failures destroy the partial PHP object immediately; successful construction keeps the existing public behavior.
The backend ownership contract is documented in `src/sampler.h`.

The earlier question about whether a bailout exits PHP needs a distinction: a request bailout can unwind to the
request runner while an FPM worker continues serving requests. PHP 8.1 wraps script execution in a bailout boundary in
[php_execute_script()](https://github.com/php/php-src/blob/PHP-8.1/main/main.c), and the
[FPM request loop](https://github.com/php/php-src/blob/PHP-8.1/sapi/fpm/fpm/fpm_main.c) runs request shutdown afterward.
Request-heap reclamation alone cannot release an unregistered native resource.

Persistent allocation is different. Inspection of PHP 8.1's
[allocator](https://github.com/php/php-src/blob/PHP-8.1/Zend/zend_alloc.c) and
[string helpers](https://github.com/php/php-src/blob/PHP-8.1/Zend/zend_string.h) showed that persistent event-name
duplication uses the system allocator, whose allocation failure exits the process. Nonpersistent event-name copies
do not allocate. The Linux native constructor therefore needs no additional request-bailout guard:
its request allocations precede descriptor acquisition, and its native failure paths release descriptors before
raising PHP diagnostics. The sampler backends likewise publish successful acquisition without another PHP allocation;
their ordinary acquisition failures release native resources before allocating diagnostic objects.

### R04 experimental evidence

The new [construction fixture](../../tests/sampler/construction.phpt) compiles the actual common sampler factory and
object destructor against a small substitute backend. It checks for a registered empty owner before acquisition and
an attached owner before reading. It also verifies no extra retained resources after ordinary open/read failures,
continued use of another sampler across those failures, separate live resources for two samplers, idempotent close,
and destructor cleanup. Before the production change, it failed with
`No empty owner before native acquisition`; afterward, it passed. These are bounded ownership and error-path checks,
not a memory-exhaustion experiment or an observed production descriptor leak.

The new Linux [partial-open cleanup test](../../tests/handle/open-failure-cleanup.phpt) checks descriptor counts after
three invalid-event failures following valid events, a subsequent successful open, and object destruction.
It characterizes existing cleanup behavior while exercising the new empty-owner failure path. It is not a regression
reproduction of the original allocation-order concern.

### R04 review and final verification

**Verdict: PASS_WITH_RESIDUAL_RISK.** Independent correctness and test reviews found no production defect in this slice.
The test review added the surviving-sampler assertion and confirmed that the Linux cleanup test also passes against
a fresh `6224216` build. In temporary copies, delaying attachment until after the initial read made the construction
fixture fail with `Native resource has no owner before read`; dropping the pointer before failure destruction made
its resource-count assertion fail. These mutations exercise only the bounded substitute backend.

Final verification after both reviews:

- `make -j2` and five focused PHPTs covering construction, partial-open cleanup, close, sampler errors, and lifetime:
  passed.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test`, with `PERFIDIOUS_TEST_OPCACHE` set to the available opcache module:
  **82 passed, 21 skipped, zero failures**.
- Four real-backend cleanup/lifetime PHPTs under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'`:
  four passed, zero reported leaks.
- The strengthened construction fixture built separately and ran directly under Valgrind against the PHP executable,
  with `USE_ZEND_ALLOC=0` and `--leak-check=full --errors-for-leak-kinds=definite --error-exitcode=99`:
  zero errors and zero bytes in use at exit. The compiler and child PHP process are not covered merely by running the
  enclosing PHPT under Valgrind, so this was a separate run.
- Composer validation, generated-stub freshness and loading, PHP_CodeSniffer, PHPStan, all four declaration-analysis
  configurations, PHP syntax, Markdown, and final diff checks: passed.

Native Windows/macOS compilation and execution, actual allocation-bailout cleanup, concurrent ZTS behavior, and full
NixOS VM integration remain unverified. The Windows factory change follows the same ownership order, but Linux tests
do not establish its native runtime behavior.

### R04 review follow-up

The external review message and `tmp.md` were both read and considered alongside a separate Codex review of the
uncommitted diff, native acquisition helpers, and object cleanup paths. No production change was needed.

The handoff identified a minor fixture limitation: `has_owner(NULL)` also matches a closed sampler retained in the
object store. The first construction and failure-isolation cases already detect the original ordering error, but the
closed `$survivor` could weaken the empty-owner check in later construction cases. The test now unsets that closed
object before opening the next samplers. This removes the ambiguous owner without adding internal state to the fixture
or changing production behavior. A bounded `WeakReference` check with the substitute backend confirmed that `close()`
leaves the object alive and `unset()` destroys it, with zero native resources at both points. The other review notes
remain verification limits, not additional fixes.

After this test change, the build, five focused PHPTs, and the full Linux PHP 8.1.34 suite passed: **82 passed,
21 skipped, zero failures**. Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan and all four
declaration-analysis configurations passed. Changed PHP syntax, Markdown, and final diff checks also passed.
The updated construction fixture ran directly under Valgrind with Zend allocation disabled: zero errors and no
allocations left at exit.

The external review also reported a passing construction fixture on PHP 8.5 NTS and ZTS debug builds. This follow-up
independently rebuilt and ran the updated fixture on the available PHP 8.5.9 ZTS debug runtime; it passed. The NTS
debug result remains the external review's evidence. Neither run establishes concurrent ZTS behavior or a full
extension suite on PHP 8.5. The handoff file was removed after evaluation and checks; nothing was committed.

## Follow-up: R05 portable configure comparisons

Implementation review base: `4873177`.

The debug, coverage, and sanitizer option predicates now use POSIX `test` equality (`=`). Their enabled and disabled
branches retain the same definitions and flags. This lets Dash honor the requested options while preserving their
behavior under Bash.

The new [configure regression check](../../tests/configure-options.py) runs `phpize` against a temporary source copy,
then configures a separate build directory for each case. It checks the generated `PERFIDIOUS_DEBUG` and `NDEBUG`
definitions and uses `make -n` to inspect every compiler/linker recipe for the requested instrumentation flags.
It covers all options enabled, all explicitly disabled, each option enabled individually, and the defaults under both
Dash and Bash: twelve cases. The check runs once in the Linux PHP 8.1 CI job, which now explicitly installs Dash and
Python 3.

Run it with a PHP development toolchain, compiler, Make, Dash, Bash, and Python 3 available:

```sh
python3 tests/configure-options.py
```

The `--dash` and `--bash` arguments accept executable paths when either shell is outside `PATH`. The check leaves the
working checkout's generated configuration and built extension untouched.

### R05 experimental evidence

Before changing `config.m4`, the new check failed in the Dash all-enabled case. Configure itself returned zero but
emitted three `unexpected operator` diagnostics; the debug definition was absent, `NDEBUG` was enabled, and the
coverage and sanitizer flags were missing from the generated recipes. After changing the three comparisons, all
twelve cases passed. An isolated mutation that made coverage depend on the debug option was rejected in the debug-only
case because coverage flags appeared when coverage had been explicitly disabled.

The existing Linux debug build was then regenerated and configured with Dash using its saved `config.nice` options,
including fatal compiler warnings. `make -j2` passed, the generated Makefile selected Dash, and loading the rebuilt
module confirmed `Perfidious\DEBUG` was true. Three focused debug/error-path PHPTs passed, followed by the full
PHP 8.1.34 suite: **82 passed, 21 skipped, zero failures**.

Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan and all four declaration-analysis
configurations passed. Python syntax, Actionlint, Markdown, and final diff checks also passed.

The configure matrix verifies definitions and generated build commands; it does not run coverage collection or an
ASan/UBSan-instrumented PHP process. Native Darwin configuration/builds and the remote CI run remain unverified.

### R05 review follow-up

Review base: `7db50dc`.

The external review message and `tmp.md` were read and considered alongside a separate Codex review of the
uncommitted comparisons, surrounding option handling, test harness, and CI selection. No actionable defect was found,
so no production or test changes were needed. The handoff's sanitizer-linker note concerns pre-existing settings;
no regression from this predicate change was demonstrated. Native instrumentation execution and remote CI remain
verification limits rather than additional changes in this slice.

Fresh verification passed all twelve Dash/Bash configure cases, the Linux build, Composer validation, generated-stub
freshness, PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations. Python syntax, Actionlint,
Markdown, and final diff checks also passed.

Both suite counts were reproduced locally. Without `PERFIDIOUS_TEST_OPCACHE`, the suite reported **80 passed,
23 skipped, zero failures**: the two FPM preload tests skipped because the opcache shared module was not found.
Providing the installed module path through that variable made both tests pass, producing **82 passed, 21 skipped,
zero failures**. This accounts for the difference between the external review's count and the earlier verification.

The handoff file was removed after evaluation and checks. Nothing was committed.

## Follow-up: R06 close-on-exec counter descriptors

Implementation review base: `0efc1c7`.

Counter descriptors now close automatically when their process executes another program. Both the dummy group leader
and each member are created with `PERF_FLAG_FD_CLOEXEC`. `rawStream()` duplicates its descriptor with
`fcntl(fd, F_DUPFD_CLOEXEC, 0)`, because plain `dup()` clears close-on-exec on the duplicate. This covers owned handles,
automatic request counters, and their raw streams through the same native opening and duplication paths.

The reason for this change is resource ownership: a program launched by PHP should not accidentally keep PHP's
counters open. Closing or destroying a handle still releases its own descriptors, and a raw stream still owns a
separate descriptor that can outlive the handle. Close-on-exec takes effect when a process executes another program.
A fork without exec still inherits descriptors.

Both operations set the flag atomically. The
[event-creation API](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) supports this flag starting with
Linux 3.14, and [atomic duplication](https://man7.org/linux/man-pages/man2/F_DUPFD.2const.html) is available starting
with Linux 2.6.24. Setting the flag later would leave a window for another thread to fork and exec. The fix uses these
operations directly without a fallback that reintroduces that window. Native errors retain the existing exception and
cleanup paths. A raw-stream duplication failure now identifies `fcntl` in its diagnostic.

### R06 experimental evidence

The new [descriptor regression](../../tests/handle/close-on-exec.phpt) first inspects `/proc/self/fdinfo`. Before the
implementation change, it found **zero close-on-exec descriptors out of ten**: two request-counter descriptors, three
owned-handle descriptors, and five raw-stream duplicates. The PHPT failed for those missing flags and stopped before
launching children. After rebuilding with the fix, all ten descriptors had the flag and the test passed.

With the corrected extension, the test launches a PHP child through `proc_open()` with an argument array, `proc_open()`
with a shell command, and `exec()`. Each child disables automatic request counters, reports **zero perf descriptors**,
and exits successfully. The parent retains all ten descriptors after those launches. The test also checks that closing
a raw stream leaves its handle usable, a raw stream remains readable after its handle closes, and closing the owned
handle and all raw streams leaves only the request-counter descriptors.

An additional `strace -e trace=perf_event_open,fcntl` run of the corrected regression confirmed five event opens using
`PERF_FLAG_FD_CLOEXEC` and five duplications using `F_DUPFD_CLOEXEC`. This checks the actual syscalls, beyond the final
flags inspected by the PHPT. The existing debug failure test now also checks that duplicating a closed descriptor
raises `Perfidious\IOException`.

Verification on Linux x86-64 with PHP 8.1.34 debug:

- `make -j2` passed with fatal compiler warnings enabled.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/handle/close-on-exec.phpt tests/handle/raw-stream.phpt tests/handle/close.phpt tests/handle/debug-close-fd.phpt tests/handle/open-failure-cleanup.phpt'` passed all five tests.
- The full suite with `PERFIDIOUS_TEST_OPCACHE` pointing to the installed opcache module passed **83 tests, with
  21 skipped and zero failures**.
- The same five focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.

### R06 reliability review

Verdict: **PASS_WITH_RESIDUAL_RISK**. The independent Breaker found no actionable correctness defect in the five-file
slice, affected callers, or cleanup paths. The independent Test Attacker reran all five focused tests twice, checked
the atomic event-opening and duplication syscalls, and found no demonstrated failure or needed test additions.
No production fix was required after these reviews.

The Breaker noted that the project had no stated minimum Linux kernel version. The new event-opening flag requires
kernel support introduced in Linux 3.14. That compatibility requirement is now explicit in the changelog.

The child-process checks establish the corrected behavior for those three local PHP launch paths. They do not
reproduce inheritance with the old implementation or cover every PHP launcher. Concurrent ZTS fork/exec, older Linux
kernels, native Windows/macOS, and remote CI remain unverified. Failure to allocate a PHP stream after successful native
duplication was not injected. The production change is confined to the Linux backend. Changes remain uncommitted for
review.

### R06 review follow-up

Review base: `0efc1c7`.

The external review message and `tmp.md` were read and considered alongside a separate Codex review of the current
diff, request-counter callers, descriptor ownership, error paths, and regression assertions. No actionable production
defect was found. No production or test changes were needed in this follow-up.

The handoff's low-priority concern about `fdinfo` reporting stale close-on-exec flags on duplicates was rejected. The
[Linux manual](https://man7.org/linux/man-pages/man5/proc_pid_fdinfo.5.html) identifies that behavior as a bug fixed in
Linux 3.1, before this patch's Linux 3.14 requirement. The
[kernel formatter](https://github.com/torvalds/linux/blob/master/fs/proc/fd.c) includes the current descriptor's
close-on-exec bit, and [anonymous-file creation](https://github.com/torvalds/linux/blob/master/fs/anon_inodes.c) masks
`O_CLOEXEC` out of the shared file flags.

A bounded local experiment opened `/dev/null` with `O_CLOEXEC`, then created one libc `dup()` and one
`F_DUPFD_CLOEXEC` duplicate. `F_GETFD` and `/proc/self/fdinfo` agreed in all three cases: the original and protected
duplicate had close-on-exec, while the plain duplicate did not. The experiment closed all three descriptors and
launched no children. The existing PHPT retains its process-boundary checks as separate behavioral coverage.

The external review's explicit stream-transfer check was independently repeated with the corrected extension.
Passing a raw stream deliberately through `proc_open()`'s descriptor specification let the child read 32 bytes and
exit successfully. The parent's stream and handle remained usable afterward. This verifies that intentional stream
transfer still works alongside prevention of accidental descriptor inheritance.

Fresh verification passed the Linux build, all five focused PHPTs, and the full PHP 8.1.34 suite with the installed
opcache module: **83 passed, 21 skipped, zero failures**. Composer validation, generated-stub freshness, PHP syntax,
PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations passed. Markdown and final diff checks
also passed. Other platform/version combinations and the earlier runtime verification limits remain unverified.

The handoff file was removed after evaluation and checks. Nothing was committed.

## Follow-up: R07 metadata identifier validation

Implementation review base: `82bdc34`.

This slice addresses the PMU/event-identifier part of R07. `get_pmu_info()`, `get_pmu_event_info()`, and
`list_pmu_events()` now validate the original PHP integer before converting it to libpfm's native identifier type.
PID bounds and CPU-ID validation remain pending for a separate slice.

The three PMU lookup paths share a checked wrapper around `pfm_get_pmu_info()`. Values below `PFM_PMU_NONE` or at or
above `PFM_PMU_MAX` return `PFM_ERR_INVAL` before any cast to `pfm_pmu_t`. Values within that range still go through
libpfm's normal support checks, including the existing unsupported-PMU result for `PFM_PMU_NONE` (zero). This follows
the [libpfm lookup contract](https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html) while preventing information
loss before libpfm sees the value.

Event indices remain `zend_long` until the internal event lookup checks `0 <= idx <= INT_MAX`. Only then does it cast
to the `int` accepted by `pfm_get_event_info()`. Enumeration already supplies native integer indices and uses the same
helper. Invalid values keep the existing `PmuNotFoundException` or `PmuEventNotFoundException` class, with libpfm's
`PFM_ERR_INVAL` code. Diagnostics now use PHP's signed-integer format and display the full original identifier.

### R07 metadata experimental evidence

The new [metadata regression](../../tests/pmu-identifiers.phpt) failed before implementation. Adding or subtracting
one 32-bit modulus from a valid identifier caused all three PMU lookup paths and the event-index lookup to accept the
altered value. Some rejected values also produced a different error code after narrowing or displayed a truncated or
unsigned value in the diagnostic. The test checks these cases through metadata APIs without opening counter handles.

After rebuilding with the fix, the same test passed. It covers negative identifiers, `PHP_INT_MIN`, `PHP_INT_MAX`,
representable nonexistent events, and, on 64-bit PHP, both signs of wraparound and the values just outside the signed
32-bit range. It also verifies the existing error for PMU zero, full values in diagnostics, and valid lookup and
enumeration results after rejected calls. When both IDs are invalid, the PMU lookup's error retains precedence over
event validation. The existing metadata tests pass without changed expectations.

Verification on Linux x86-64 with PHP 8.1.34 debug and libpfm 4.13.0:

- `make -j2` passed with fatal compiler warnings enabled.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/pmu-identifiers.phpt tests/get-pmu-info.phpt tests/get-pmu-event-info.phpt tests/list-pmus.phpt tests/list-pmu-events.phpt tests/list-pmu-events-unknown-pmu.phpt'` passed all six tests.
- The full suite with `PERFIDIOUS_TEST_OPCACHE` pointing to the installed opcache module passed **84 tests, with
  21 skipped and zero failures**.
- The same six focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.

### R07 metadata reliability review

Verdict: **PASS_WITH_RESIDUAL_RISK**. The independent Breaker found no actionable correctness defect. The independent
Test Attacker found no production failure and strengthened the regression with two cases where both identifiers are
invalid. These protect the existing PMU-before-event error precedence, including PMU zero's unsupported result.
The strengthened six-test suite passed. No further production changes were needed.

A 32-bit PHP runtime, other PHP/libpfm versions, native Windows/macOS, and remote CI were not exercised for this slice.
The PMU/event association issue in R09 is separate and remains pending. Changes remain uncommitted for review.

### R07 metadata review follow-up

Review base: `82bdc34`.

The external review message and `tmp.md` were read and considered alongside a separate Codex review of the current
diff, metadata callers, native conversions, error paths, and regression assertions. Both reviews found no actionable
defect. No production or test changes were needed in this follow-up.

The handoff's include-order suggestion was left unchanged. `src/private.h` already depended on libpfm types before
this patch, and every current Linux caller includes `pfmlib.h` first. No affected caller was found. The regression's
fixed PMU identifier and error codes match the existing metadata tests and installed libpfm definitions. PID/CPU
validation remains a separate R07 slice, and checking whether an event belongs to the requested PMU remains R09.

A fresh metadata experiment enumerated all available PMUs and events, then compared each object with a direct lookup
using its identifiers. All **386 PMUs and 18,393 events** round-tripped without a mismatch. This checks that the new
bounds preserve valid metadata beyond the software PMU used in the regression.

Fresh verification on Linux x86-64 with PHP 8.1.34 debug and libpfm 4.13.0 passed the build, all six focused PHPTs,
and the full suite with the installed opcache module: **84 passed, 21 skipped, zero failures**. Composer validation,
generated-stub freshness, PHP syntax, PHP_CodeSniffer, PHPStan and all four declaration-analysis configurations passed.
Markdown and final diff checks also passed. Other platforms, 32-bit PHP, other PHP/libpfm versions, and remote CI
remain unverified. The earlier Valgrind run was not repeated because this follow-up made no production or test edits.

The handoff file was removed after evaluation and checks. Nothing was committed.

## Follow-up: R07 PID and CPU validation

Implementation review base: `81540cd`.

This slice completes the remaining R07 validation changes. PID conversion now checks both bounds of the configured
native `pid_t` before casting a wider PHP integer. Values outside that range raise `OverflowException`, with the
original signed value in the diagnostic. Representable negative PIDs retain their existing native validation path.

CPU arguments below `-1` now raise an argument-specific `ValueError`. Values above `INT_MAX` retain
`OverflowException`, and the diagnostic reports the actual native integer limit. The online CPU-count check was
removed. [`sysconf(_SC_NPROCESSORS_ONLN)`](https://man7.org/linux/man-pages/man3/sysconf.3.html) reports a count, so it
cannot establish whether a particular CPU ID exists when numbering has gaps. Representable CPU IDs now reach
[`perf_event_open()`](https://man7.org/linux/man-pages/man2/perf_event_open.2.html), which validates availability and
permissions. The `-1` CPU sentinel and PID capability checks retain their existing behavior.

The Linux declaration documents the PID/CPU constraints and `ValueError`, and the aggregate stub was regenerated.
The two existing positive-overflow PHPTs now skip when PHP integers are only 32 bits, because their `PHP_INT_MAX`
inputs cannot exceed the native Linux PID or CPU integer width in that configuration.

### R07 PID and CPU experimental evidence

The new [argument regression](../../tests/handle/open-identifier-bounds.phpt) first failed against the unchanged
implementation. `PHP_INT_MIN` and a value just below the native PID minimum reached event validation instead of
raising `OverflowException`. CPU `-2` and `PHP_INT_MIN` also reached event validation, while CPU `INT_MAX` was
rejected using the online count despite fitting the native type.

The same test passed after the fix. It covers both PID bounds, CPU values below `-1` and above `INT_MAX`, preservation
of original values in overflow diagnostics, and accepted integer boundaries. Every call supplies a non-string event
element so any argument that passes integer validation stops before native acquisition. In a debug build, the test
also confirms the native-open call count is unchanged. No alternate process or CPU counter was opened by these cases.

A separate bounded experiment replaced only the child process's `sysconf()` online-count result with two. The
corrected extension let CPU ID seven reach event validation, with zero native-open calls. This models the admission
check for a sparse numbering case without changing host CPU topology. The PHPT protects the same
distinction by requiring CPU `INT_MAX` to reach event validation. Neither check establishes successful kernel counter
acquisition on a physically sparse or offline CPU topology.

Verification on Linux x86-64 with PHP 8.1.34 debug:

- `make -j2` passed with fatal compiler warnings enabled.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/handle/open-identifier-bounds.phpt tests/handle/open-fails-invalid-pid.phpt tests/handle/open-fails-invalid-cpu.phpt tests/handle/open-fails-non-string-event-no-open.phpt tests/handle/open-failure-cleanup.phpt tests/pmu-identifiers.phpt'` passed all six tests.
- The full suite with the installed opcache module passed **85 tests, with 21 skipped and zero failures**.
- The same six focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.
- PHP syntax, changed C-fragment formatting, Markdown, and final diff checks passed.

### R07 PID and CPU reliability review

Verdict: **PASS_WITH_RESIDUAL_RISK**. The independent Breaker found no actionable correctness defect. The independent
Test Attacker found no production failure and strengthened the regression to protect the exact positive PID boundary,
rejection before narrowing, and the expected event-validation error after representable arguments. The positive PID
boundary case runs in debug builds, which bypass the capability check. All added cases stop before native acquisition.

The strengthened six-test suite, full suite, Valgrind checks, and static checks were rerun successfully after review.
No further production changes were needed.

A 32-bit PHP runtime, other PHP versions, native Windows/macOS, physical CPU-topology changes, and remote CI remain
unverified. Non-debug capability behavior was not executed, including error precedence for a positive PID paired with
an invalid CPU. That check retains its existing position before CPU validation. Changes remain uncommitted for review.

## Follow-up: R08 referenced event strings

Implementation review base: `322f7e2`.

`Perfidious\open()` now dereferences each local event-value pointer before checking its type and extracting the string.
This uses the same `ZVAL_DEREF()` pattern as the shared sampler. It accepts references whose current value is a string,
including references left by a `foreach` loop. The caller's array and reference bindings are preserved.

The existing handle factory retains its own string reference for each accepted event. Reassigning or unsetting the
caller's referenced variable after opening therefore leaves the handle's original event names intact. Non-string
values still raise `TypeError` before native acquisition, and objects with `__toString()` are not coerced.

### R08 experimental evidence

The new [reference regression](../../tests/handle/event-name-references.phpt) first failed against the unchanged
implementation: a reference to a dynamically constructed valid event name raised `TypeError: All event names must
be strings`. After the fix and rebuild, it passed for both an explicit reference and an array processed by reference
iteration. The test reassigns the caller's variables, verifies that their reference bindings still work, unsets them,
and checks that the handle still returns the original event names.

The initial [invalid-value test](../../tests/handle/event-name-references-invalid.phpt) passed before and after the
fix. Its final version protects rejection of referenced nulls, booleans, integers, floats, arrays, resources, and
stringable objects, and verifies that rejected references keep their identity and bindings.
Each invalid value follows a valid event name in the list. The debug native-open counter confirms that validation
rejects the entire list before acquisition. The stringable object's conversion method throws if called, so the test
also detects unwanted coercion.

Verification on Linux x86-64 with PHP 8.1.34 debug:

- `make -j2` passed with fatal compiler warnings enabled.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/handle/event-name-references.phpt tests/handle/event-name-references-invalid.phpt tests/handle/event-name-lifetime.phpt tests/handle/open-fails-non-string-event.phpt tests/handle/open-fails-non-string-event-no-open.phpt tests/handle/open-failure-cleanup.phpt tests/sampler/sampler-reference-metrics.phpt'` passed all seven tests.
- The full suite with the installed opcache module passed **87 tests, with 21 skipped and zero failures**.
- The same seven focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.
- PHP syntax, Markdown, and final diff checks passed.

### R08 reliability review

Verdict: **PASS_WITH_RESIDUAL_RISK**. The independent Breaker found no actionable correctness defect and confirmed the
reference, lifetime, and rejection behavior with direct probes. The independent Test Attacker found no production
failure and strengthened both PHPTs with reference-identity and binding checks, resource rejection, and bounded
allocation churn after caller values are unset. These checks protect against replacing caller references, accepting
non-string values through coercion, or retaining event names without ownership.

The strengthened seven-test suite, full suite, Valgrind checks, and static checks were rerun successfully after review.
No further production changes were needed.

Other PHP versions, release builds, 32-bit PHP, native Windows/macOS, and remote CI were not exercised for this slice.
Changes remain uncommitted for review.

## Follow-up: R09 PMU/event ownership

Implementation review base: `a67269f`.

`Perfidious\get_pmu_event_info()` now checks that the event's PMU matches the requested PMU before constructing the
result. A mismatch raises `PmuEventNotFoundException` with code `PFM_ERR_NOTFOUND` (`-4`) and a message identifying
the requested event index and PMU. An event that exists in libpfm's global database is not necessarily an event of
the requested PMU.

PMU validation still precedes event lookup. Existing bounds checks and errors for invalid identifiers retain their
behavior. The Linux stub documents the ownership requirement, and the aggregate stub has been regenerated.

### R09 experimental evidence

A direct call against the unchanged implementation requested an event owned by PMU 7 (`netburst`) using PMU 8
(`netburst_p`). It returned owner ID 7 with the incorrect name `netburst_p::TC_deliver_mode`.

The new [mismatch regression](../../tests/get-pmu-event-info-mismatched-pmu.phpt) first failed against that
implementation. It accepted an event from PMU 7 for PMU 51, and an event from PMU 51 for PMU 7. After the ownership
check and rebuild, both calls raised the expected exception. The test chooses PMUs from the installed database and
prefers different presence flags when available. It also checks that valid lookups and enumeration still return
consistent metadata after each rejected call.

A separate metadata-only experiment enumerated all **386 PMUs and 18,393 events** in the installed libpfm 4.13.0
database. Every direct lookup matched its enumerated result, including the owner ID, PMU name prefix, and presence
flag. These checks do not require opening hardware counters.

Verification on Linux x86-64 with PHP 8.1.34 debug:

- `make -j2` passed with fatal compiler warnings enabled.
- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/get-pmu-event-info-mismatched-pmu.phpt tests/get-pmu-event-info.phpt tests/get-pmu-info.phpt tests/list-pmu-events.phpt tests/list-pmu-events-unknown-pmu.phpt tests/list-pmus.phpt tests/pmu-identifiers.phpt tests/get-pmu-event-info-long-name.phpt tests/readonly-pmu-event-info.phpt'` passed all nine tests.
- The full suite with the installed opcache module passed **88 tests, with 21 skipped and zero failures**.
- The same nine focused tests under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.
- PHP syntax, changed C-fragment formatting, Markdown, and final diff checks passed.

### R09 reliability review

Verdict: **PASS_WITH_RESIDUAL_RISK**. The independent Breaker found no actionable correctness defect and checked
mismatch rejection and recovery across all 18,393 installed events. The independent Test Attacker found no production
failure in a separate probe of the first and last events of each nonempty PMU: **764 cross-owner rejections and 764
valid lookups** passed. This sampled event boundaries across 385 PMUs, without testing every possible PMU/event pair.

The Test Attacker strengthened the regression to check the owner, name prefix, and presence flag for every enumerated
event in the two selected PMUs. Re-enumeration after each mismatch must preserve those results. The strengthened
nine-test suite, full suite, Valgrind checks, and static checks were rerun successfully after review. No further
production changes were needed.

Other PHP/libpfm versions, release builds, 32-bit PHP, native Windows/macOS, and remote CI were not exercised for this
slice. Changes remain uncommitted for review.

## Follow-up: R10 non-zero counter assertion

Implementation review base: `675a486`.

The [non-zero counter test](../../tests/handle/non-zero-after-enable.phpt) now selects the requested event from
`Handle::readArray()` and requires its value to be an integer greater than zero. A missing event fails the assertion.
The test runs a CPU hash loop with a 5 ms deadline after enabling the handle, then reads the result and closes the
handle. This replaces the sleep loop with CPU work, following the existing reset-state test's workload pattern.

The Linux-only test still requires usable native perf counters. It does not turn a zero reading or an open/read error
into a skip. Linux CI configures perf permissions before running the suite. A failure on another host needs diagnosis
of both the extension and the native counter environment. This slice changes the test and this report only.

### R10 experimental evidence

The original test passed before editing. Both `['counter' => 0] > 0` and `[] > 0` also returned `true` in a direct PHP
probe. More significantly, a real handle left disabled returned an event value of `0` after CPU work, yet comparing
the returned array with zero still produced `true`.

To check assertion sensitivity, a temporary Python driver extracted the PHPT's actual `--FILE--` body and ran it
through PHP with the built extension. It compared stdout with the PHPT's expected output. These were temporary
in-memory variants of the test body, with no changes to the extension or its native reads:

| Test-body variant | Original assertion | Corrected assertion |
| --- | --- | --- |
| Live enabled counter | `bool(true)`, passes | `bool(true)`, passes |
| Omit `enable()`, leaving the real counter disabled | `bool(true)`, false positive | `bool(false)`, fails |
| Substitute an event value of zero at the read-result boundary | `bool(true)`, false positive | `bool(false)`, fails |
| Substitute an empty read result | `bool(true)`, false positive | `bool(false)`, fails |
| Substitute a numeric string (`'1'`) as the event value | Not run | `bool(false)`, fails |

All variants exited normally without stderr. The negative variants fail because of the assertion result, not an
unrelated exception or setup failure. The unchanged live run verifies native counting separately from the result
substitutions. This is a test correction, with before/after sensitivity checks rather than a production behavior fix.

Verification on Linux x86-64 with PHP 8.1.34 debug:

- `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/handle/non-zero-after-enable.phpt tests/handle/enable.phpt tests/handle/disable.phpt tests/handle/read.phpt tests/handle/reset-enabled.phpt tests/handle/zero-after-reset.phpt tests/handle/sequential-reads.phpt'` passed all seven tests.
- The full suite with the installed opcache module passed **88 tests, with 21 skipped and zero failures**.
- The changed PHPT under `USE_ZEND_ALLOC=0 make test TEST_PHP_ARGS='-n -m'` passed with zero reported leaks.
- Composer validation, generated-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed.
- PHP syntax, Markdown, and final diff checks passed.

### R10 test review

Verdict: **KEEP**. The WIO strategy and test reviews found no required edits. The review used the Test Level Selection,
Test Oracles And Assertions, and Mutation Testing references to assess the native integration boundary and assertion
sensitivity. The corrected test detects a missing enable operation or an unusable returned counter, which the original
assertion accepted.

The independent test reviewer ran the extracted test body successfully **20 times**. Omitting `enable()` failed
**three times**. Additional in-memory probes for a disabled/reset handle, zero, empty results, a wrong event key, and
string, float, or boolean values each produced `bool(false)`. The final changed PHPT also passed under Valgrind.
These repetitions provide local evidence, without establishing stability on every kernel or heavily starved host.

Other PHP versions, release builds, 32-bit PHP, perf-restricted or non-counting hosts, and remote CI were not exercised
for this slice. The workload deadline uses wall time, so heavy preemption can reduce the CPU work performed. The test
checks positivity only, without establishing measurement accuracy or a minimum elapsed CPU interval.
Changes remain uncommitted for review.

The findings, source line numbers, and examples below describe the reviewed revision identified above. Examples using
the removed global API require that revision; they are retained as historical experimental evidence.

## Evidence overview

| ID | Issue | Evidence status |
| --- | --- | --- |
| R01 | Persistent FPM counters target the initializing process | Reproduced with two local FPM workers |
| R02 | Darwin process CPU time exposes Mach ticks as nanoseconds | Source trace and compiled Linux sampler shim; no native macOS run |
| R03 | Reset counts are scaled with lifetime timing fields | Live reset behavior and actual scaling output verified; multiplex timing supplied by a fixture |
| R04 | Allocation bailout can strand native resources before ownership transfer | Factories reordered; controlled ownership/error-path fixture passed; no allocation-failure experiment |
| R05 | Dash silently disables requested instrumentation | Fixed; twelve Dash/Bash configure cases and a Dash-configured Linux debug build passed |
| R06 | Counter descriptors lack close-on-exec flags | Fixed; ten descriptor flags and three PHP child-launch paths checked, with atomic syscalls confirmed |
| R07 | Identifier validation narrows values or rejects sparse CPU IDs | Metadata and PID/CPU bounds fixed with regressions; simulated sparse admission verified, physical sparse topology untested |
| R08 | Referenced event strings are rejected | Fixed; reference acceptance, bindings, name lifetime, and non-string rejection verified |
| R09 | PMU/event lookup can combine unrelated metadata | Fixed; mismatches rejected in both directions and all 18,393 installed events round-tripped with consistent ownership |
| R10 | Non-zero counter assertion compares an array with zero | Fixed; live counting passes, while disabled, zero, empty, and wrong-typed results fail the corrected assertion |
| R11 | INI metric lists use unbounded stack allocation | Source concern; no oversized configuration executed |
| R12 | CI log selection and Codecov metadata are incorrect | Log selection and corrected pipeline verified with fixtures; external Codecov outcome unchecked |

R01–R05 deserve attention first because they affect measurement correctness or the reliability of runtime and build
behavior. The remaining findings are smaller correctness, hardening, test, and maintenance issues. These priorities are
not claims of demonstrated security exploitation.

## R01: Persistent counters are created before FPM workers fork

**Locations:** [src/linux/platform.c:167](../../src/linux/platform.c#L167),
[src/linux/platform.c:178](../../src/linux/platform.c#L178),
[src/handle.c:328](../../src/handle.c#L328).

Both persistent groups are opened during MINIT. The helper uses `pid=0, cpu=-1`, selecting the calling task. FPM then forks
its workers, which inherit descriptors for those original events. Request hooks reset, enable, and disable the existing
groups; they never create a group targeting the worker. This conflicts with the documented worker/request semantics.
The task selection and inheritance rules are described in [perf_event_open(2)](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).

**Experiment:** a temporary FPM pool used two static workers and a Unix socket. Both persistent handles were enabled in
FPM's startup configuration with `perf::PERF_COUNT_SW_TASK_CLOCK:u`. Four sequential requests each performed about 150 ms
of CPU work, comparing the persistent handles with a new handle created inside the request and with `getrusage()`.

| Worker PID | Global delta | Request delta | Fresh handle delta | getrusage CPU delta, ns |
| --- | ---: | ---: | ---: | ---: |
| 9920 | 0 | 0 | 134537965 | 149955000 |
| 9921 | 0 | 0 | 145028195 | 149533000 |
| 9920 | 0 | 0 | 148208498 | 149638000 |
| 9921 | 0 | 0 | 141806482 | 149680000 |

An earlier concurrent attempt had one fresh control counter remain zero, so it was not used as decisive evidence.
The sequential run established advancing control counters in both workers. Counter magnitudes are host-dependent;
the useful observation is the persistent counters remaining unchanged while both independent controls advance.

The following is a compact request body for that comparison. It assumes the two persistent handles are enabled at FPM
startup and configured for the same event; running it as a one-shot CLI script does not exercise the prefork behavior.

```php
<?php
$event = 'perf::PERF_COUNT_SW_TASK_CLOCK:u';
$fresh = Perfidious\open([$event])->enable();
$handles = [
    'global' => Perfidious\global_handle(),
    'request' => Perfidious\request_handle(),
    'fresh' => $fresh,
];
$before = [];
foreach ($handles as $name => $handle) {
    $before[$name] = $handle->readArray()[$event];
}

$deadline = hrtime(true) + 150_000_000;
while (hrtime(true) < $deadline) {
    hash('sha256', 'local review workload');
}

$deltas = [];
foreach ($handles as $name => $handle) {
    $deltas[$name] = $handle->readArray()[$event] - $before[$name];
}
$fresh->close();
echo json_encode(['pid' => getmypid(), 'deltas' => $deltas]), "\n";
```

**Recommended change:** create persistent groups after entering each worker, or detect process changes and recreate them
before use. Verify worker attribution and isolation, including request reset behavior. The existing
[VM test](../../flake.nix#L260) deliberately checks lifecycle rather than counter magnitudes and uses only one worker;
that test is useful but cannot establish measurement attribution. Shared cross-worker control operations follow from
the descriptor ownership model; this experiment did not separately quantify their interference.

## R02: Darwin process CPU time has the wrong unit

**Locations:** [src/darwin/functions.c:264](../../src/darwin/functions.c#L264),
[src/darwin/sampler.c:151](../../src/darwin/sampler.c#L151).

The low-level API assigns `ri_user_time` and `ri_system_time` directly to properties named `userTimeNs` and `systemTimeNs`.
The common sampler also adds the raw fields as its nanosecond CPU-time value. Apple's implementation obtains these
fields from Mach-time accounting, and libproc returns them without conversion. See Apple's
[task accounting](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/task.c#L6391),
[rusage assignment](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c#L1196),
[kernel return path](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c#L3267), and
[libproc wrapper](https://github.com/apple-oss-distributions/xnu/blob/main/libsyscall/wrappers/libproc/libproc.c#L129).

**Experiment:** a temporary C wrapper included the existing
[Darwin sampler harness](../../tests/darwin/sampler-probe-harness.c), which compiles the real sampler implementation
against substitute native calls. The original harness passed first. One additional successful `proc_pid_rusage()`
fixture supplied `ri_user_time=18,000,000` and `ri_system_time=6,000,000`; opening and reading a process CPU-time sampler
returned `24,000,000`. Compilation used PHP development headers and `-std=c11 -Wall -Wextra -Werror` and succeeded.
The executable printed:

```text
Darwin sampler probe harness passed
sampler CPU time: 24000000
oracle at 1/1: 24000000
oracle at 125/3: 1000000000
```

**Verification limit:** this Linux experiment confirms the sampler's unchanged-value behavior, not the operating
system's units. The two oracle lines are arithmetic comparisons; the harness did not emulate a Mach timebase API.
The inference that conversion is needed relies on the upstream source trace above. Neither a native macOS process nor
the low-level PHP Darwin wrapper was executed. A unit timebase hides the issue, and tests that only require increasing
values cannot detect it.

**Recommended change:** share the existing overflow-checked Mach-to-nanosecond conversion between process and thread
paths. Initialize its timebase independently of `thread_selfcounts` availability. Test a non-unit ratio and compare
process CPU deltas against an independent `getrusage()` oracle converted from seconds/microseconds. Preserve the existing
overflow checks rather than introducing an unchecked `ticks * numer` multiplication.

## R03: Reset counts and timing fields cover different intervals

**Locations:** [src/linux/platform.c:213](../../src/linux/platform.c#L213),
[src/linux/platform.c:271](../../src/linux/platform.c#L271),
[src/handle.c:231](../../src/handle.c#L231).

Request hooks reset counts, while `phpinfo()` scales them with the enabled/running times returned by the kernel. Those
times remain cumulative across reset. This behavior is explicitly documented for `PERF_EVENT_IOC_RESET` in
[perf_event_open(2)](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).

**Experiment:** a software task-clock counter was enabled, exercised, disabled, and read before and after `reset()`.
Observed values were:

```text
             count       timeEnabled   timeRunning
before reset 49,983,407   49,983,407    49,983,407
after reset           0  49,983,407    49,983,407
```

The minimal public-API check is:

```php
<?php
$event = 'perf::PERF_COUNT_SW_TASK_CLOCK:u';
$handle = Perfidious\open([$event])->enable();
$deadline = hrtime(true) + 50_000_000;
while (hrtime(true) < $deadline) {
    hash('sha256', 'review');
}
$handle->disable();
$before = $handle->read();
$handle->reset();
$after = $handle->read();
var_dump($after->values[$event] === 0);
var_dump($after->timeEnabled === $before->timeEnabled);
var_dump($after->timeRunning === $before->timeRunning);
$handle->close();
```

**Additional experiment:** the existing [scaling fixture helper](../../tests/inject-scaling-read.inc) and debug hook
supplied a well-formed two-entry reading with raw count `50`, enabled time `200`, and running time `150`. The actual
`phpinfo(INFO_MODULES)` row and the independent interval calculation printed:

```text
perf::PERF_COUNT_SW_CPU_CLOCK:u => 50 => 66 => 75%
interval oracle: 100
```

The second line was calculated independently with `intdiv(50 * (200 - 100), 150 - 100)`. A previous interval with
enabled/running times `100/100` leaves a new interval of `100/50`, so the interval estimate is `100`; the formatter
produces `66` from lifetime timing fields. The fixture retained both event IDs and set both the internal leader and
event count to `50`; only the named event is displayed.

**Verification limit:** the reset experiment used real kernel counters, while this formatter experiment supplied
synthetic cumulative times. Changing hardware multiplex ratios were not induced. Together these checks establish the
reset semantics and the scaling calculation, without claiming an observed hardware scheduling history.

**Recommended change:** record timing baselines whenever a logical reset occurs, or calculate complete interval deltas
from cumulative counts and times. Preserve or explicitly revise the public raw-timing contract. Add a deterministic
test whose scheduling ratios differ across intervals.

## R04: Native resources precede their PHP cleanup owner

**Locations:** [src/windows/functions.c:630](../../src/windows/functions.c#L630),
[src/sampler.c:451](../../src/sampler.c#L451), [src/functions.c:290](../../src/functions.c#L290).

Several factories acquire native resources and only then allocate the PHP object responsible for releasing them.
If the later allocation hits the request memory limit, Zend can bail out before ownership is attached. The ordinary
error path and object destructor cannot clean up a handle that is still only in a C local. Reclaiming the request heap
does not close a file descriptor or disable a native profiling session.

The relevant contracts are in the [PHP allocator](https://github.com/php/php-src/blob/PHP-8.4/Zend/zend_alloc.c) and
Microsoft's [profiling cleanup documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-disablethreadprofiling).

**Verification limit:** this is a source-level ownership concern. No memory-limit failure, worker resource accumulation,
or persistent Windows profiling failure was experimentally induced. Ordinary Valgrind success does not exercise this
allocation-bailout path.

**Recommended construction order:**

```text
allocate and register the PHP owner with empty native state
acquire native resources, publishing each successful acquisition to that owner
perform further allocations and populate the result
let the registered cleanup release any partially acquired state on failure
```

The follow-up above implements this order. Inspect allocations inside native construction and diagnostic paths
as well as the final wrapper allocation. Moving `object_init_ex()` earlier is insufficient if a helper makes further
request allocations while acquired native resources remain unpublished. Where those allocations cannot move earlier
and ownership cannot be published incrementally, consider narrowly scoped bailout cleanup that releases the native
resources and propagates the bailout.

## R05: Dash silently disables requested instrumentation

**Location:** [config.m4:127](../../config.m4#L127), including the following coverage and sanitizer conditionals.

The three branches use `test "$option" == "yes"`. Dash rejects `==` in this context. The condition fails, but configure
continues and exits successfully. This is conditional on the shell actually selected; Autoconf may select Bash in other
environments.

**Experiment:** a temporary checkout was configured with Dash and all three options enabled. Configure returned exit
status zero, emitted three `unexpected operator` messages, left `PERFIDIOUS_DEBUG` undefined, and omitted both coverage
and sanitizer compiler flags. The isolated setup can be repeated on a Linux development host with Dash, PHP development
tools, libcap, and libpfm installed:

```sh
review_dash=$(command -v dash) || exit 1
review_dir=$(mktemp -d)
git archive HEAD config.m4 m4 src php_perfidious.h | tar -x -C "$review_dir"
(
    cd "$review_dir" || exit 1
    phpize && CONFIG_SHELL="$review_dash" ./configure \
        --enable-perfidious-debug \
        --enable-perfidious-coverage \
        --enable-perfidious-sanitize
)
```

Inspect the generated definitions and compile rules rather than relying on exit status. The predicate itself was also
tested: `test yes == yes` returned status `2` under Dash; `test yes = yes` returned `0`.

**Recommended correction:** use portable equality or Autoconf shell helpers, for example:

```sh
if test "$PHP_PERFIDIOUS_DEBUG" = "yes"; then
    # Emit the debug definition.
    :
fi
```

**Correction check:** a separate temporary checkout changed only the three equality operators to `=`, regenerated
configure with `phpize`, and reran the same three options under Dash. Both commands exited zero. The generated files
contained `#define PERFIDIOUS_DEBUG 1`, `-fprofile-arcs -ftest-coverage`, `--coverage`, and
`-fsanitize=address,undefined`; no `unexpected operator` diagnostic remained. Those experiments did not patch the
reviewed source; the R05 follow-up above applies the correction.

Add configuration checks that assert the requested instrumentation is present. These experiments checked generation of
flags; they did not compile or execute an instrumented sanitizer build.

## R06: Counter descriptors lack close-on-exec flags

**Locations:** [src/handle.c:368](../../src/handle.c#L368),
[src/handle.c:428](../../src/handle.c#L428), [src/handle.c:555](../../src/handle.c#L555).

The event opens pass zero flags, and `rawStream()` uses `dup()`. Neither requests close-on-exec behavior.

**Experiment:** with one event and one raw stream open, read-only inspection of the process's `/proc/self/fdinfo` entries
found three perf descriptors—the group leader, member, and duplicate. All three lacked the close-on-exec bit.

**Verification limit:** this establishes descriptor flags, not every PHP child-launch path. A particular launcher may
close descriptors explicitly. Inheritance through `proc_open()`, `exec()`, or another PHP execution API was not tested,
so the earlier blanket phrasing about child programs should be read with that qualification.

The kernel supports atomic close-on-exec event creation as described in
[perf_event_open(2)](https://man7.org/linux/man-pages/man2/perf_event_open.2.html). A remediation sketch is:

```c
/* Check return values and retain the existing failure cleanup. */
fd = perf_event_open(&attr, pid, cpu, group_fd, PERF_FLAG_FD_CLOEXEC);
duplicate_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
```

Use atomic creation/duplication rather than setting the descriptor flag afterward, which leaves an inheritance window
in a threaded process. Add an isolated process-boundary check to establish the behavior of supported PHP launchers.

## R07: Native identifier validation is incomplete

**Locations:** [src/functions.c:244](../../src/functions.c#L244),
[src/private.h:83](../../src/private.h#L83), [src/functions.c:87](../../src/functions.c#L87),
[src/pmu_info.c:82](../../src/pmu_info.c#L82).

There are three related cases:

- PMU/event IDs can be narrowed from PHP integers before libpfm validates them.
- PID conversion checks its upper bound but not its lower bound when `zend_long` is wider than `pid_t`.
- CPU validation uses the number of online CPUs as a maximum identifier, and does not fully validate representability
  before casting. An online count is not a CPU-ID set on a sparse/offline topology.

**Experiment:** metadata-only calls using a valid PMU/event ID plus one 32-bit modulus returned the original PMU/event
instead of rejecting the out-of-range argument. Both `get_pmu_info()` and `get_pmu_event_info()` showed this aliasing on
64-bit PHP. No alternate process or CPU target was opened during this experiment.

**Verification limit:** PID lower-bound behavior and sparse CPU rejection remain source findings. The host's CPU topology
was not changed, and a 32-bit PHP build was not run. This does not establish a kernel authorization bypass.

**Recommended change:** validate the domain and destination width before conversion, consistently across entry points.
For CPU arguments, a simple validation shape is:

```c
if (cpu < -1 || cpu > INT_MAX) {
    zend_value_error("CPU must be -1 or a nonnegative value representable as int");
    return;
}
/* Let perf_event_open validate whether the representable CPU ID exists. */
```

This is a proposed fragment, not a complete patch. PID checks must cover both bounds of the actual destination type;
PMU checks should use libpfm's valid identifier domain. Add lower-bound and sparse-topology coverage beside the existing
positive-overflow tests.

## R08: Referenced event strings are rejected

**Location:** [src/functions.c:273](../../src/functions.c#L273).

The event-list loop checks the stored zval type directly. An array element can contain a reference whose value is a
string, including after ordinary by-reference iteration. The API promises a list of strings, but rejects this case.
The shared sampler already dereferences its metric values before validating them.

**Experiment:** a plain one-element event list opened successfully; the same string passed by reference produced
`TypeError: All event names must be strings`.

```php
<?php
$event = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';
$plain = Perfidious\open([$event]);
$plain->close();

try {
    $referenced = Perfidious\open([&$event]);
    $referenced->close();
} catch (TypeError $error) {
    echo $error->getMessage(), "\n";
}
```

**Recommended change:** dereference the local element pointer before type checking and extracting its string.
Test both references to valid strings and references to invalid values.

## R09: PMU/event lookup returns inconsistent metadata

**Location:** [src/pmu_event_info.c:103](../../src/pmu_event_info.c#L103).

The event is looked up by its global event index, independently of the supplied PMU. The result constructor combines
that event with the supplied PMU's name and presence flag without verifying that they belong together.

**Experiment:** requesting PMU `8` with an event belonging to PMU `7` returned a result with `pmu=7`, but its name used
the PMU `8` prefix: `netburst_p::TC_deliver_mode`. These specific IDs/names describe the libpfm database on this host;
they are not portable constants.

The following metadata-only example selects two PMUs from the installed database:

```php
<?php
$pmus = array_values(array_filter(
    Perfidious\list_pmus(),
    static fn($pmu) => $pmu->nevents > 0,
));
if (count($pmus) < 2) {
    throw new RuntimeException('This check needs two PMUs with events');
}
[$first, $second] = $pmus;
$event = Perfidious\list_pmu_events($first->pmu)[0];
$result = Perfidious\get_pmu_event_info($second->pmu, $event->idx);
var_dump($second->pmu, $result->pmu, $result->name);
```

**Recommended change:** reject mismatched PMU/event pairs or derive the PMU metadata from the event's actual owner.
Add a regression check that a returned object's name, owner, and presence flag refer to the same PMU.

## R10: The non-zero counter test does not inspect a counter

**Location:** [tests/handle/non-zero-after-enable.phpt:17](../../tests/handle/non-zero-after-enable.phpt#L17).

The test compares the entire array returned by `readArray()` with zero. PHP's cross-type comparison makes this succeed
even when no positive counter exists.

**Experiment:** both statements below printed `bool(true)`:

```php
<?php
var_dump(['counter' => 0] > 0);
var_dump([] > 0);
```

**Recommended change:** assert on the numeric event value, for example `$values[$event] > 0`, after a bounded CPU
workload. Separately identify environments where native perf counting is unavailable or unreliable. Preserve useful
deterministic lifecycle checks, but do not interpret them as evidence that counters advance or measure the right task.

## R11: INI metric lists bypass the stack-allocation bound

**Location:** [src/linux/platform.c:127](../../src/linux/platform.c#L127).

The public event-list API limits event count, but the INI path splits the complete string and allocates a pointer array
of that size with `alloca()`. There is no equivalent count bound before the native stack allocation. A sufficiently
large configuration therefore has a different failure mode from an ordinary rejected event list.

**Verification limit:** source inspection only. No oversized configuration or stack-exhaustion attempt was executed.
The settings are `PHP_INI_SYSTEM`, so this is an administrator-configuration hardening concern, not an established
remote-input vulnerability.

**Recommended change:** share the count limit across entry points or use a checked heap allocation for the temporary
array. Verify bounded rejection and cleanup without relying on a process crash as the expected behavior.

## R12: CI diagnostics and coverage metadata need correction

### Docker log selection

**Location:** [.github/scripts/docker.sh:14](../../.github/scripts/docker.sh#L14).

`find tests -print0 -name '*.log'` performs the output action before applying the filename predicate. The error handler
therefore feeds directories and unrelated files to `cat`, obscuring the actual failure.

**Experiment:** its discovery command selected 128 paths in this checkout; all 128 were non-log paths. Applying the
filter first selected zero log files, which matched the test directory's state. The paths were counted, not executed.

A corrected Linux shell form is:

```sh
find tests -type f -name '*.log' -print0 | xargs -0 -r cat --
```

This form targets the script's Linux Docker workflow; `xargs -r` is not portable to every non-GNU environment.

**Additional experiment:** an isolated fixture contained two log files, one with a space in its name, an unrelated PHP
file, a nested directory, and a directory ending in `.log`. The original discovery command emitted all six paths.
The corrected pipeline emitted exactly the two log contents, produced no stderr, and exited zero. After removing the
two fixture log files, it again exited zero with no output. This verifies selection, quoting, directory exclusion, and
the empty-log case; it did not launch Docker or trigger a real CI job failure.

### Codecov repository slug

**Locations:** [.github/workflows/ci.yml:348](../../.github/workflows/ci.yml#L348) and
[ci.yml:461](../../.github/workflows/ci.yml#L461).

Both upload steps specify `jbboehr/php-perfifidous` rather than `jbboehr/php-perfidious`.
The typo is directly visible in the configuration. Recent upload outcomes were not checked against Codecov, so rejected
or misattributed uploads remain a possible consequence rather than an observed service failure.

```yaml
slug: jbboehr/php-perfidious
```

## Additional improvements and unresolved questions

### Deterministic Windows failure and counter-width tests

The [Windows sampler](../../src/windows/sampler.c) has native acquisition, read, cleanup, and 32-bit widening paths.
Existing Windows PHPTs largely exercise live successful calls and normal lifecycle behavior. Add controlled native-call
fixtures for acquisition failures, recoverable read errors, independent counter wraps, and cleanup. The
[Darwin shim](../../tests/darwin/sampler-probe-harness.c) provides an existing local pattern. This is a coverage
recommendation; it does not imply every untested branch is defective.

### Persistent handles under a threaded ZTS SAPI

Inspect initialization of module-global handles in actual worker threads. A CLI binary compiled with ZTS exercises
thread-aware compilation but does not establish multi-threaded request behavior. This remains a coverage question
related to R01. No threaded embedding/SAPI experiment was performed.

### 32-bit Linux support and page-fault width

[Context-switch handling](../../src/linux/sampler.c#L162) explicitly widens 32-bit counters, while
[page-fault handling](../../src/linux/sampler.c#L148) rejects negative signed values. Clarify the intended 32-bit support
contract and test native-width boundaries accordingly. This was not exercised on 32-bit Linux and is not counted as a
separately reproduced defect.

### Windows sampler close failures

[Sampler cleanup](../../src/windows/sampler.c#L255) discards the profiling-disable result, whereas the low-level
`ThreadProfile::close()` retains ownership when disable fails. However, the design already requires thread-scoped
samplers to be closed on the same thread, and no supported normal-use failure was established. Keep this as a lifecycle
question; do not claim an additional normal-use leak without that evidence. Native Windows execution was unavailable.

### Debug descriptor invalidation

`Handle::debugCloseFd()` deliberately invalidates native state for failure-path tests. Its behavior can complicate a
test if another descriptor is opened and reuses the number before cleanup. This is intentional debug-only mutation,
not a confirmed production finding. When extending such tests, make descriptor lifetime explicit and avoid accidentally
testing reuse of an unrelated resource.

### Exception documentation

[The design document](../SAMPLER_API.md#lifecycle-and-errors) says a non-Metric input should throw `ValueError`, whereas
the implementation and test expect `TypeError`. The follow-up experimentally confirmed:

```php
<?php
try {
    Perfidious\Sampler::open(['cpu-time']);
} catch (Throwable $error) {
    echo get_class($error), ': ', $error->getMessage(), "\n";
}
```

Observed output: `TypeError: All metrics must be instances of Perfidious\Metric`. Update the prose to match the intended
contract. Also distinguish implemented semantics from proposed future work and make the platform/metric support matrix
easy to find.

### Developer experience and code maintenance

Add a short development guide linked from [CONTRIBUTING.md](../../CONTRIBUTING.md). Contributors should be able to find
Composer checks, PHPT execution, debug hooks, VM tests, and the optional static sanitizer build without reconstructing
them from CI, Nix, and the changelog. Keep those instructions in maintainer documentation.

Prefer targeted fixes over a broad refactor. The shared sampler/backend separation, immutable outputs, explicit
ownership flags, reference-lifetime tests, and runtime-versus-stub contract check are useful existing structure.
Generated native arginfo may eventually reduce declaration duplication, but is an optional maintenance improvement;
retain the public-contract tests if adopting it.

## Baseline checks and limits

At the reviewed revision, the earlier baseline checks passed:

- Linux debug build with PHP 8.1.34 and warnings treated as errors.
- Full PHPT suite: 98 tests, 78 passed, 20 skipped, zero failures.
- Full PHPT suite under Valgrind with Zend allocation disabled: 78 passed, 20 skipped, zero failures and zero reported
  test leaks. This is the standard PHPT memory check, not an exhaustive process-wide leak proof.
- Composer strict validation, aggregate stub freshness, PHP syntax checks, PHP_CodeSniffer, PHPStan, and the four API
  declaration analysis configurations.
- Actionlint, repository ShellCheck, Nix formatting, Markdown lint, and diff whitespace checks.

The follow-up runtime experiments used the same revision; the configure correction was tested only in a temporary
checkout with the three documented operator substitutions. The compiled Darwin shim exercised project code with
substitute native calls on Linux. These checks did not include native Windows/macOS, the complete multi-version Nix
matrix, 32-bit PHP, or ASan/UBSan. The FPM check was a temporary local pool, not a rerun of the project's full NixOS VM
tests. Memory exhaustion and oversized INI input remain untested concerns. No use-after-free, double-free, or heap-buffer
defect was confirmed; these checks do not prove their absence.

The evidence table deliberately retains partial or source-only statuses for R02, R04, R06, R07, R11, and the Codecov
portion of R12. In particular, descriptor flags do not establish every launcher's inheritance behavior, and successful
ordinary cleanup does not establish allocation-bailout cleanup. Those gaps must not be counted as passed experiments.

The final focused PHPT run selected `tests/info-scaling.phpt` and `tests/darwin/sampler-probe-shim.phpt`: both passed,
with zero failures and zero skips. These existing tests validate the fixture infrastructure and ordinary behavior;
the additional observations in R02 and R03 come from the separate experiments described there.

For this document, all six PHP examples passed syntax checks and the five standalone Linux PHP examples executed with
the documented results. The FPM example is a compact version of the request body used in the temporary pool experiment.
All three shell examples passed syntax checks; the C remediation fragments are schematic and were not compiled.
Markdown lint and local-link checks passed.

The native review assigned 30 local files, 223 units, and 6,669 lines, including generated `config.h`, and produced all
five source-review parts plus two cross-cutting sweeps. The parser recovered with errors in 27 files, mostly around PHP
macros. All 531 ledger questions received responses, but only 516 passed the consistency gate: 24 violations affected
15 questions. Another 29 units containing 734 lines generated no questions, so the gate cannot establish their coverage.
Reviewers reported reading those scopes, and the extra sweeps were not restricted to parsed sites.

The ledger is a consistency check, not independent proof of coverage; reviewers retained shell access. No separate
false-positive or severity review ran, and native severity assessments remain subject to correction. The raw assembler
reported 18 entries because it included six unresolved pointers alongside 12 filed native findings. Those entries are
not 18 experimentally confirmed or distinct defects. This report combines overlapping findings and keeps open questions
separate. Generated output symlinks `result` and `result-dev` were excluded because they resolve outside the repository;
no remaining unreadable path was reported.
