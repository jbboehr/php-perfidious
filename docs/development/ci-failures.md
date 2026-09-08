# CI failure investigation

Updated on 2026-09-07, based on `554278a`. The Linux Valgrind failure was a reproducible false positive in the native
fixture's CPU-time assertion. The corrected fixture compares perf with the parent's own CPU clock. Both Apple Silicon
macOS jobs now pass; the fixture correction has not run in CI.

## Linux Valgrind test

The [PHP 8.4 debug job](https://github.com/jbboehr/php-perfidious/actions/runs/34165305069/job/101875059586)
passes its ordinary suite, then fails `tests/sampler/linux-perf-live.phpt` under Valgrind 3.22.0. It reports one failed
test and zero leaked tests. The same fixture failed under Valgrind in the
[previous PHP 8.4 run](https://github.com/jbboehr/php-perfidious/actions/runs/34162546500/job/101867145810) and an
[earlier PHP 8.3 run](https://github.com/jbboehr/php-perfidious/actions/runs/34158960211/job/101856569724).
The failure therefore predates S02 and S03 and is not confined to PHP 8.4.

The original diagnostic collector missed nested test logs. The recursive collector now exposes the failure in the
[latest PHP 8.2 job](https://github.com/jbboehr/php-perfidious/actions/runs/34171289979/job/101891934398):

```c
after.values[PERFIDIOUS_METRIC_CPU_TIME] - before.values[PERFIDIOUS_METRIC_CPU_TIME] <
    worker_delta.values[PERFIDIOUS_METRIC_CPU_TIME] / 5
```

This failed at line 142 of [the live fixture](../../tests/sampler/linux-perf-live.c) in `554278a`. The ordinary suite
passes; Valgrind reports one failed test and zero leaked tests. Matrix fail-fast cancels six other Linux jobs.

### Cause

The parent interval includes `pthread_create()`, `pthread_join()`, and sampler reads. The worker runs for about
100 ms of its own CPU time. The assertion therefore assumes parent setup and cleanup always consume less than
roughly 20 ms. Thread isolation does not imply that bound: instrumentation and processor throughput can increase
the parent's CPU cost without adding any worker time to its counter.

Early local runs passed under Nix's Valgrind 3.22.0 and 3.26.0. Switching to Ubuntu's packages brought the parent
much closer to the threshold. Adding load on the sibling hardware thread reproduced the original failure with
CI's Valgrind options.

### Experimental evidence

The reproduction used the unmodified fixture at `554278a`, compiled with GCC 15.2 and PHP 8.1.34 headers on Linux
x86-64. It ran in a temporary Bubblewrap filesystem containing Ubuntu's `valgrind` 3.22.0-0ubuntu3 and `libc6`/
`libc6-dbg` 2.39-0ubuntu8.8, matching the failing job's packages. The ELF loader and library path selected that
runtime. The host remained Linux 7.1.5 on a Ryzen 9 9950X3D; this did not reproduce GitHub's kernel or processor.

Valgrind arguments matched PHP's test runner:

```sh
valgrind -q --tool=memcheck --trace-children=yes \
    --vex-iropt-register-updates=allregs-at-mem-access ./linux-perf-live
```

Pinning the fixture to CPU 0 passed without added load. A bounded Python integer loop pinned to CPU 16, its sibling
hardware thread on this host, made the unmodified fixture fail at line 142 in all three runs. These runs used no
extra Valgrind options. The load competes for execution resources; time spent descheduled is not the CPU time
being measured.

A temporary diagnostic copy bracketed each parent sampler read with `CLOCK_THREAD_CPUTIME_ID` calls. Subtracting
the inner and outer clock readings bounds the parent CPU time between the two perf samples. All three loaded
runs failed the original assertion while perf remained inside those independent bounds:

| Run | Parent perf time | Parent CPU-clock bounds | Worker perf time | Assertion limit |
| --- | ---: | ---: | ---: | ---: |
| 1 | 24.244 ms | 22.849–24.709 ms | 105.103 ms | 21.021 ms |
| 2 | 24.166 ms | 22.744–24.628 ms | 104.881 ms | 20.976 ms |
| 3 | 23.934 ms | 22.593–24.450 ms | 104.935 ms | 20.987 ms |

Thread creation alone consumed 20.874–21.130 ms in those runs. The measured parent time agrees with its own clock
and cannot include the worker's approximately 105 ms. This demonstrates the assertion's false positive.
Ubuntu Valgrind with `--track-origins=yes` also failed three runs without added CPU load; parent perf readings
of 27.569–27.881 ms remained within their CPU-clock bounds.

The GitHub log does not contain measured values, so the runner's exact timings remain unknown. These diagnostic
experiments preceded the fixture correction below.

### Fixture correction

The thread-isolation check now compares the parent's perf delta with `CLOCK_THREAD_CPUTIME_ID`, using the fork
check's existing 20% tolerance. The parent performs 100 ms of CPU work after joining the worker, so sampler-read
overhead remains small relative to the interval. Worker metric and wrong-thread checks remain in place. Worker
deltas are local because the parent no longer uses them as a timing limit. A shared assertion prints the phase,
perf reading, clock reading, and tolerance when either isolation check fails. Production code is unchanged.

Before editing, a fresh build reproduced the original assertion failure under the Ubuntu Valgrind runtime and
sibling-thread load described above. After editing, the same setup passed all three runs, plus an idle run and
an idle run with origin tracking.

Temporary fixture copies also exercised two deliberate defects through real perf events. Both failed at the
expected CPU-time assertion, natively and under Ubuntu Valgrind with sibling-thread load:

| Defect | Failing check | Perf reading under load | Parent CPU clock |
| --- | --- | ---: | ---: |
| Set `perf_event_attr.inherit = 1`, adding worker CPU time | Thread isolation | 238.290 ms | 124.352 ms |
| Disable each event before closing it, including the child's inherited descriptors | Fork isolation | 2.048 ms | 103.674 ms |

The Linux PHP 8.1.34 rebuild and focused sampler suite passed: 7 tests passed and 12 skipped. The full ordinary suite
passed 84 tests with 34 skips and zero failures. Skips cover native platforms, kernel-inclusive perf permissions,
ZTS, release-only behavior, and OpCache prerequisites. Composer validation, stub freshness, PHPCS, and all documented
PHPStan checks, configured lint hooks, C formatting, and local documentation links passed. The correction has not
run on GitHub or under other PHP versions, ZTS, or kernel-inclusive perf access.

### Separate FPM timing failure

The full Valgrind 3.26.0 suite passed the corrected sampler fixture, but finished with 83 passes, 34 skips, one
failure, and zero leaked tests. The failure is in [the FPM worker test](../../tests/request-handle/fpm-worker.py):

```python
check(result["start"] < result["fresh"] / 2, result)
```

The first request's initial counter was 53.372 ms; the fresh counter measured 101.141 ms, giving a 50.571 ms limit.
Running only this unchanged FPM fixture reproduced the failure: 60.183 ms against a 50.642 ms limit. That isolated
run does not execute the modified sampler fixture. Request and fresh workload deltas agreed, and second-request
initial counters were below 1 ms. The FPM assertion's timing assumptions need a separate investigation and correction.

## Intel macOS PHP installation

The [macOS PHP 8.1 job](https://github.com/jbboehr/php-perfidious/actions/runs/34162546500/job/101867145864)
fails inside `setup-php`, before configuration or compilation, after roughly 32 minutes. The installer reports only
`Could not setup PHP 8.1`; its Homebrew output is redirected away.

The action maintainer has [announced the end of Intel macOS support](https://github.com/shivammathur/setup-php/issues/1112).
The [PHP 8.1 formula rebuild](https://github.com/shivammathur/homebrew-php/commit/92592eeae09505d0a09b4db763f62e67bd153422)
also removed its Intel macOS bottle. The `macos-15-intel` job depended on an unsupported installation path.
The suppressed output prevents identification of the final Homebrew error in this run.

Official macOS support is now limited to Apple Silicon with ARM64 PHP. Both PHP 8.1 and 8.5 CI jobs use `macos-15`,
which GitHub [documents as ARM64](https://docs.github.com/en/actions/reference/runners/github-hosted-runners#standard-github-hosted-runners-for-public-repositories).
This follows the PHP installer's recommendation and avoids maintaining a separate Intel installation path.
Intel source builds remain untested and unsupported; no architecture guard was added.

## Diagnostics change

[CI](../../.github/workflows/ci.yml) now recursively prints `.log`, `.diff`, and `.mem` files with their paths in the
macOS, Linux test, and coverage jobs. An empty diagnostic directory succeeds without adding a second failure.

One-off Bash fixtures reproduced the original collector's missing nested logs and empty-directory failure. The command
from the updated workflow passed both cases and a mixed case with top-level logs, nested diffs, Valgrind reports,
spaces in paths, and an unrelated PHP file.

A parsed workflow comparison confirmed two macOS jobs, PHP 8.1 and 8.5 on `macos-15`, with their existing build and test
steps. Only the macOS matrix and diagnostic collectors changed. Configured lint hooks, Composer validation, declaration
freshness, PHPCS, all documented PHPStan checks, local link targets, and `git diff --check` passed.
The recursive collector exposed the assertion in both subsequent PHP 8.2 failures. Native macOS builds and tests
passed in the latest [PHP 8.1 job](https://github.com/jbboehr/php-perfidious/actions/runs/34171289979/job/101891934395)
and [PHP 8.5 job](https://github.com/jbboehr/php-perfidious/actions/runs/34171289979/job/101891934353).
