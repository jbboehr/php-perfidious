# Linux perf sampler rewrite

Review base: `f91e65df44758589936f8fcc8dbfb2713ccfa803`.

## Contract

`Sampler::open()` defaults to `Scope::CurrentThread` on every platform. Linux replaces its process `getrusage()`
backend with per-thread perf events for CPU time, page faults, context switches, CPU cycles, and retired instructions.
Linux rejects explicit `CurrentProcess` requests. Windows and macOS retain their existing process metrics when callers
select that scope explicitly. Thread CPU time is the shared metric across all three platforms.

The Linux events include user and kernel execution, exclude hypervisor execution, and do not inherit into other
threads or child processes. Kernel-inclusive permissions are required; permission errors remain `IOException`.
Unsupported native events produce structured `UnsupportedMetricException` metadata. The backend does not silently
change scope, exclude kernel execution, or substitute another counter source.

Each event has its own enabled/running times. Successful reads scale and accumulate the interval since the previous
successful read, preserving earlier estimates when the multiplexing ratio changes. Failed native reads leave all interval
baselines intact. Scaling rounds down, and arithmetic overflow throws. Independent events can observe different
execution intervals under multiplexing; ratios between those estimates are approximate.

## Scope evidence

A native two-thread experiment ran about 100 ms of CPU work in an existing worker while the main thread waited.
Across three runs, a perf task-clock counter targeting the worker measured 100.038–100.073 ms. Counters targeting
`pid=0` and `pid=getpid()` in the main thread measured 0.014–0.047 ms. Passing a process ID did not aggregate the worker.
The experiment used user-only filters because this host denies kernel-inclusive events.

Perf also supports CPU-wide and cgroup monitoring and inheritance to new tasks. Those modes do not make a single
attachment to the main thread include all existing process threads. See
[`perf_event_open(2)`](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).

## Verification

The following results apply to Linux x86-64 with PHP 8.1.34 unless stated otherwise.

- Before implementation, the new native contract test failed because Linux rejected thread metrics. The changed
  reflection test failed because the default was still `CurrentProcess`. Both passed after implementation.
- A temporary extension built from the original shared sampler and Linux backend rejected the new PHP thread-default
  test for the expected scope mismatch.
- The controlled perf fixture checks all five mappings, unsupported-event and permission classification, acquisition
  cleanup before diagnostics, interrupted and short reads, failed-read recovery, wrong-thread reads, changing scaling
  ratios, backwards counters, zero running time, and overflow. Late failures preserve every metric's baseline and leave
  the output unchanged; retries recover the full interval. Cleanup leaves another sampler usable.
- The live native fixture runs the production backend with only its kernel-exclusion flag changed. It checks real
  CPU time, page faults, available hardware metrics, worker attribution, and wrong-thread errors. A fork test verifies
  that child work is excluded and closing inherited descriptors leaves the parent's counters running.
  Context-switch events cannot advance under that filter and are excluded from this restricted-host fixture.
- Both native fixtures passed Valgrind with zero reported errors, zero remaining allocations, and only the three
  inherited standard descriptors open. Both also passed ASan/UBSan. The sanitizer fixture build undefines GCC's initial
  `__SANITIZE_ADDRESS__` macro so the PHP 8.1 header can define it without a redefinition diagnostic; instrumentation
  remains enabled.
- The host suite passed **83 tests with 34 skipped**, with no failures or malformed tests. The skips include live
  sampler tests requiring kernel-inclusive access; controlled and user-only native fixtures ran.
- Composer validation, aggregate declaration freshness, PHP_CodeSniffer, all five documented PHPStan invocations,
  configured lint hooks, local documentation links, and seven PHP documentation syntax checks passed.
- The PHP 8.1 release VM suite passed **82 tests with 35 skipped** under software emulation. The Linux thread-metric,
  context-switch, and error tests ran with normal kernel-inclusive permissions. Both unprivileged FPM preload tests
  passed separately, and the Nginx/FPM lifecycle checks passed. The emulated VM does not expose hardware counters.

Reliability verdict: **PASS_WITH_RESIDUAL_RISK**. The independent reviews found no production defects or accepted static
findings. The adversarial test pass strengthened interval recovery, arithmetic, requested-only probes,
and sampler independence checks. Temporary mutations that ignored the request mask, published partial output, or
divided before multiplying all failed the strengthened tests. A separate mutation that disabled events during close
failed the live fork test when the parent's counter stopped advancing.

Native Windows/macOS execution, kernel-inclusive hardware counting, KVM execution, and other PHP versions remain
unverified by this initial run. Native fixtures do not establish PHP ZTS integration or full-extension sanitizer coverage;
the follow-up below checks those paths separately.

Focused checks:

```sh
NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='tests/sampler tests/lifecycle-exceptions.phpt'
NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test
```

The two native fixtures were also compiled separately with `cc -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror -pthread`,
PHP's include flags, and the repository include directory. Valgrind used
`--error-exitcode=99 --leak-check=full --track-fds=yes`. Sanitizer builds added
`-g -O1 -U__SANITIZE_ADDRESS__ -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`.

The VM was built through `checks.x86_64-linux.php81-gcc-vmtest.driver`, then run with
`QEMU_OPTS='-cpu max -machine accel=tcg -display none'`. The driver retained `phpt.log`, `phpt-preload.log`, and
`phpt-preload-results.txt`. The standard KVM target and native fixture prerequisites are documented in the
[development guide](guide.md).

## Follow-up: full-extension ASan/UBSan

Review base: `1c4d3771ca2e0d1e5152d4d25607a9fc2aeafcb3`. Verification on 2026-09-07 found no ASan/UBSan defects and
led to the test-only correction below. All three fresh Linux x86-64 builds passed with PHP 8.2.32 and GCC 15.2.0,
both before and after that correction:

| Extension / PHP thread mode | Host passed | Host skipped |
| --- | ---: | ---: |
| Release / NTS | 72 | 45 |
| Debug / NTS | 82 | 35 |
| Debug / ZTS | 76 | 41 |

Each suite contained 117 tests, with zero warnings, failures, or sanitizer reports. Ten tests per host suite skipped
because `kernel.perf_event_paranoid=2` denied the sampler's kernel-inclusive events. All seven FPM tests, including both
preload modes, passed in both NTS variants. The concurrent request-handle fixture passed in ZTS.

```sh
nix build --no-link --max-jobs 3 --cores 8 -L \
  .#sanitize-static-php82-check \
  .#sanitize-static-php82-debug-check \
  .#sanitize-static-php82-zts-debug-check
```

Build logs showed all ten extension source files compiled with `-fsanitize=address,undefined` and
`-fno-sanitize-recover=all` in every variant. All three PHP executables linked both runtimes, and disassembly of
`perfidious_platform_sampler_read()` contained calls to both sanitizers. The built sampler source and regression
fixtures matched the checkout byte for byte.

The checks set `USE_ZEND_ALLOC=0`, `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1`, and
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`. PHP core, OpCache, and dependencies are not instrumented.
LeakSanitizer is disabled, so these runs add no leak-detection evidence. Native PHPT harnesses do not inherit the
extension's instrumentation flags; the ZTS helper is explicitly instrumented.

### Kernel-inclusive sampler checks

A temporary NixOS VM used the same instrumented executables with `kernel.perf_event_paranoid=1`. Tests ran as an
unprivileged user under software emulation, with four virtual CPUs and 4 GiB of memory. ASan's verbose preflight
confirmed runtime initialization and successfully opened, read, and closed a current-thread CPU-time sampler.

Each variant then ran:

```sh
php -n run-tests.php --show-diff tests/sampler tests/lifecycle-exceptions.phpt
```

The focused VM run passed **16 tests with four skipped** in each of release NTS, debug NTS, and debug ZTS.
All kernel-inclusive software sampler tests ran. The skips were the replacement-extension construction fixture and
three native Windows/macOS tests. The VM exposes no hardware PMU.

A temporary copy of the existing ZTS worker workload additionally opened CPU-time, page-fault, and context-switch
samplers in each worker request. Staggered CPU work checked active-worker attribution and exclusion of the idle
worker's peer against `CLOCK_THREAD_CPUTIME_ID`. The workload exercised explicit close and automatic request-shutdown
cleanup; the existing native runner checked descriptor restoration after thread teardown. Both the normal run and a
run restricted to one virtual CPU passed, with no sanitizer reports.

These experiments used the existing sanitizer flags and the instrumented ZTS helper. They do not establish behavior
in a production threaded SAPI, native Windows/macOS execution, or hardware-counter coverage under the full-extension
sanitizer builds.

An attempted full sanitizer suite in the temporary VM did not return and was interrupted after approximately twelve
minutes. The VM results above cover the focused sampler/lifecycle suite; the full-suite results are from the host.

### Fork-oracle correction

The VM exposed a timing assumption in the uninstrumented native fixture, without a sanitizer report. The original fork
check required perf's count to stay below an exact bound from a separately sampled thread clock during a short idle
parent interval. It failed in **12 of 30** repeated runs. Diagnostic runs measured positive differences of up to
65,563 ns, enough to fail that bound without implying that the parent's counter included the child's 100 ms workload.

The fixture now performs its existing parent CPU workload before the comparison and accepts a 20% difference between
the two clocks. This keeps accounting jitter small relative to the measured interval. It requires the parent's counter
to advance and still rejects the additional CPU time that inheriting the child's work would contribute. The revised
fixture passed **30 of 30** VM runs and a native ASan/UBSan run with leak detection enabled.

Temporary mutations confirmed that the revised fork assertion rejects both inherited child work and disabling the
parent's event when the child closes its inherited descriptors. The inheritance mutation bypassed the earlier worker
assertion only in its temporary test copy, so the fork assertion was checked independently. No production code changed.

After applying the fixture correction, the ordinary Linux PHP 8.1 suite passed **83 tests with 34 skipped**, and all
three full sanitizer suites passed again with the counts above. Configured lint hooks, local documentation links, and
diff whitespace checks passed. The applied fixture matched the candidate used in the VM and mutation checks.
