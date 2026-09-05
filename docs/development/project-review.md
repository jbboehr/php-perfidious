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

The findings, source line numbers, and examples below describe the reviewed revision identified above. Examples using
the removed global API require that revision; they are retained as historical experimental evidence.

## Evidence overview

| ID | Issue | Evidence status |
| --- | --- | --- |
| R01 | Persistent FPM counters target the initializing process | Reproduced with two local FPM workers |
| R02 | Darwin process CPU time exposes Mach ticks as nanoseconds | Source trace and compiled Linux sampler shim; no native macOS run |
| R03 | Reset counts are scaled with lifetime timing fields | Live reset behavior and actual scaling output verified; multiplex timing supplied by a fixture |
| R04 | Allocation bailout can strand native resources before ownership transfer | Source concern; no allocation-failure experiment |
| R05 | Dash silently disables requested instrumentation | All three options reproduced; portable correction verified in an isolated checkout |
| R06 | Counter descriptors lack close-on-exec flags | Live descriptor flags inspected; inheritance through PHP child-launch APIs not tested |
| R07 | Identifier validation narrows values or rejects sparse CPU IDs | PMU/event aliasing reproduced; PID/CPU bounds and sparse topology remain source findings |
| R08 | Referenced event strings are rejected | Reproduced through the public PHP API |
| R09 | PMU/event lookup can combine unrelated metadata | Reproduced through the public PHP API |
| R10 | Non-zero counter assertion compares an array with zero | Reproduced with both a zero-valued array and an empty array |
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

This is a design sketch, not an implemented fix. Inspect allocations inside native construction and diagnostic paths
as well as the final wrapper allocation. Simply moving `object_init_ex()` earlier is insufficient if native resources
remain unpublished until a helper returns. Where ownership cannot be published incrementally, consider narrowly scoped
bailout cleanup that releases the native resources and propagates the bailout.

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
`-fsanitize=address,undefined`; no `unexpected operator` diagnostic remained. The project's source was not patched.

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
