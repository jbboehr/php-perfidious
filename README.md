
# php-perfidious

[![ci](https://github.com/jbboehr/php-perfidious/actions/workflows/ci.yml/badge.svg)](https://github.com/jbboehr/php-perfidious/actions/workflows/ci.yml)
[![Codecov](https://codecov.io/gh/jbboehr/php-perfidious/graph/badge.svg?token=DSLDXIWHC5)](https://codecov.io/gh/jbboehr/php-perfidious)
[![License: AGPL-3.0-only WITH romic-exception](https://img.shields.io/badge/license-AGPL--3.0--only%20WITH%20romic--exception-blue.svg)](LICENSE.md)
[![Tag](https://img.shields.io/github/v/tag/jbboehr/php-perfidious)](https://github.com/jbboehr/php-perfidious/releases)
![stability-experimental](https://img.shields.io/badge/stability-experimental-orange.svg)
[![AI burn](https://img.shields.io/endpoint?url=https%3A%2F%2Fgist.githubusercontent.com%2Fjbboehr%2F553936a99fb41cf6dc90a5fee06a2516%2Fraw%2Fagent-badge.json&cacheSeconds=300)](https://github.com/arlegotin/agent-badge)

This extension provides a common sampler for a small set of process and thread metrics, plus low-level access to Linux
`perf_events` and experimental Windows and macOS performance APIs.

## Requirements

* 64-bit PHP 8.1 - 8.5; 32-bit builds are unsupported
* Linux: libcap and libpfm4
* Windows: 64-bit x64 PHP on a Windows version supported by that PHP release
* macOS: process and current-thread resource snapshots are available; hardware cycle and instruction counts may be
  unavailable on older or virtualized systems

## Installation

### PIE

PIE installation is currently Linux-only. PIE installs precompiled extension DLLs on Windows,
and this project does not publish those release artifacts yet.

Install the build toolchain and required system libraries first. On Ubuntu and Debian:

```bash
apt install build-essential git libcap-dev libpfm4-dev php-dev
```

After [installing PIE](https://php.github.io/pie/#installing-pie), install the current development version from a source
checkout:

```bash
git clone https://github.com/jbboehr/php-perfidious.git
cd php-perfidious
pie install
```

### Source

The commands below cover Linux. Windows builds use the matching PHP SDK and Visual Studio toolchain with
`phpize.bat`, `configure.bat --enable-perfidious`, and `nmake`.

You will need a few packages, including libcap and libpfm4. On Ubuntu and
Debian, this should be:

```bash
apt install build-essential git libcap-dev libpfm4-dev php-dev
```

Now clone the repo and compile the extension:

```bash
git clone https://github.com/jbboehr/php-perfidious.git
cd php-perfidious
phpize
./configure
make
make test
sudo make install
````

Add the extension to your *php.ini*:

```ini
extension=perfidious.so
```

Finally, *restart the web server*.

## Usage

See also the [`examples`](./examples) directory and the [`stub`](./perfidious.stub.php).

### Cross-platform sampler

`Sampler` measures the current native thread by default. CPU time is available on Linux, Windows, and macOS.
Linux also supports page faults, context switches, CPU cycles, and instructions, subject to perf permissions and hardware
availability.
See the [support matrix](docs/SAMPLER_API.md#support-matrix) for all implemented platform and scope combinations.

```php
use Perfidious\Metric;
use Perfidious\Sampler;

$sampler = Sampler::open([Metric::CpuTime]);

try {
    $before = $sampler->read();

    $digest = hash('sha256', str_repeat('x', 1_000_000));

    $delta = $sampler->read()->since($before);
    printf("CPU time: %d ns\n", $delta->value(Metric::CpuTime));
} finally {
    $sampler->close();
}
```

The sampler begins counting when it is opened. Each sample is cumulative from that point, while `since()` returns the
difference between two samples from the same sampler.

On Windows and macOS, pass `Scope::CurrentProcess` to measure the whole process. Linux supports `Scope::CurrentThread`
only. Linux sampler metrics include kernel execution and require permission for kernel-inclusive perf events; permission
failures throw `IOException`. See [troubleshooting](#troubleshooting).

Metrics use string-backed enums, so configuration values map directly through `Metric::from()`. Generic reporters can
inspect `Metric::unit()` to distinguish nanoseconds from counts. Opening a sampler validates the complete request. If
the selected platform, scope, or host cannot provide a metric,
`UnsupportedMetricException` reports the scope and rejected metrics through its `$scope` and `$unsupportedMetrics`
properties.

Use the APIs below when you need counters or native details outside the common sampler.

### Linux perf_events

`Perfidious\open()` accepts arbitrary libpfm event names and exposes Linux perf-event timing and multiplexing details.

```php
$handle = Perfidious\open(["perf::PERF_COUNT_SW_CPU_CLOCK:u"]);
try {
    $handle->enable();

    for ($i = 0; $i < 3; $i++) {
        var_dump($handle->readArray());
        sleep(1);
    }
} finally {
    $handle->close();
}
```

```text
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
  int(3190)
}
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
  int(51270)
}
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
  int(86560)
}
```

`read()` and `readArray()` return raw counts since opening the handle or its latest `reset()`.
The `ReadResult::timeEnabled` and `timeRunning` fields are kernel-lifetime totals in nanoseconds; `reset()` does not
clear them. To scale counts yourself after a reset, disable the handle and save its timing totals immediately before
resetting, then subtract those totals from later readings. `phpinfo()` applies this timing baseline automatically.
Resetting an enabled handle briefly pauses counting and resumes it after the reset.

Or you can configure an automatic per-request handle:

```php
// with the following INI settings:
// perfidious.request.enable=1
// perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
try {
    var_dump(Perfidious\request_handle()?->read());
} catch (Perfidious\PmuEventNotFoundException | Perfidious\IOException $error) {
    error_log($error->getMessage());
}
```

```text
object(Perfidious\ReadResult)#%d (%d) {
  ["timeEnabled"]=>
  int(260840)
  ["timeRunning"]=>
  int(260840)
  ["values"]=>
  array(3) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
    int(142740)
    ["perf::PERF_COUNT_SW_PAGE_FAULTS:u"]=>
    int(64)
    ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u"]=>
    int(0)
  }
}
```

### Windows

The Windows API currently exposes low-level counters in `Perfidious\Windows`:

```php
$cycles = Perfidious\Windows\query_current_process_cycle_time();
$times = Perfidious\Windows\get_current_process_times();
$threadTimes = Perfidious\Windows\get_current_thread_times();
$memory = Perfidious\Windows\get_current_process_memory_info();

$cpuTime100ns = $times->kernelTime100ns + $times->userTime100ns;
$pageFaults = $memory->pageFaultCount;

$profile = Perfidious\Windows\enable_current_thread_profiling();
try {
    $before = $profile->read();
    usleep(1000);
    $after = $profile->read();

    $contextSwitches = $after->contextSwitchCount - $before->contextSwitchCount;
    $cpuCycles = $after->cycleCount - $before->cycleCount;
} finally {
    $profile->close();
}
```

`ProcessTimes` and `ThreadTimes` distinguish their creation `FILETIME` timestamps from
the kernel and user CPU durations, whose property names include their 100-nanosecond unit.
`ProcessMemoryInfo` reports `PROCESS_MEMORY_COUNTERS_EX`; despite its native name,
`pagefileUsage` is process commit charge, while `privateUsage` is private committed memory.

`ThreadProfileSnapshot` contains cumulative context switches, normalized CPU cycles,
the wait-reason bitmap observed since the previous native read, per-read retry metadata,
and optional hardware counters. Hardware counters are selected with a bitmask of up to
16 globally configured indices and require a Windows kernel driver. A requested but
unconfigured index reads as zero, which is indistinguishable from a configured counter
that observed no events; `hardwareCounterCount` reports how many entries Windows says
are populated.

### macOS

The macOS API exposes cumulative process and current-thread resource snapshots in `Perfidious\Darwin`:

```php
$process = Perfidious\Darwin\get_current_process_resource_usage();
$thread = Perfidious\Darwin\get_current_thread_resource_usage();

$processCpuTimeNs = $process->userTimeNs + $process->systemTimeNs;
$threadCpuTimeNs = $thread->userTimeNs + $thread->systemTimeNs;
```

The process snapshot also contains page-fault and context-switch counts. Cycle and instruction counts may be zero when
macOS cannot collect them, including on some older or virtualized systems.

## Events

The event-name API in this section is Linux-only.

We use the libpfm4 event name encoding to open events. To see a list of all events,
execute [examples/all-events.php](examples/all-events.php) with the extension loaded
or see the [libpfm4 documentation](https://perfmon2.sourceforge.net/docs_v4.html).
Some notable generic perf events are:

* `perf::PERF_COUNT_HW_CPU_CYCLES:u`
* `perf::PERF_COUNT_HW_INSTRUCTIONS:u`
* `perf::PERF_COUNT_SW_PAGE_FAULTS:u`
* `perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u`

## Configuration

| Name | Default | Changeable | Description |
| --------------------- | -------- | ----------- | ------------ |
| `perfidious.request.enable` | `0` | `PHP_INI_SYSTEM` | Set to `1` to enable the per-request handle. This handle is kept open between requests, but reset before and after. See the [Linux example](#linux-perf_events) for reading it and handling errors. |
| `perfidious.request.metrics` | `perf::PERF_COUNT_HW_CPU_CYCLES:u`, `perf::PERF_COUNT_HW_INSTRUCTIONS:u` | `PHP_INI_SYSTEM` | The metrics to monitor with the request handle. |

The request handle opens in the worker on its first request, is kept open between requests under a persistent-worker
SAPI like php-fpm, and is reset at the start and end of each request. Under the CLI SAPI, one invocation is one PHP
request; use an explicitly owned handle from
`Perfidious\open()` to choose measurement intervals within a long-running script.

If opening fails, the next request retries. Configuration and operating-system errors are reported when
`request_handle()` is called: invalid metric names throw `PmuEventNotFoundException`, and perf access or lifecycle
failures throw `IOException`. Each pending error is reported once; later calls in the same request return `null` if
the handle remains unavailable.
An unconsumed error remains pending across requests, even if a later initialization attempt succeeds. Once it is
reported, another call can return the recovered handle.

`Perfidious\global_handle()`, `perfidious.global.enable`, and `perfidious.global.metrics` have been removed. Remove those
settings from existing configuration. Automatic cumulative counters across requests are no longer provided;
`request_handle()` measures individual PHP requests.

## Troubleshooting

**Q:** I get an error `pid greater than zero and CAP_PERFMON not set`

**A:** You need to grant `CAP_PERFMON` when monitoring a process other than the
current process, for example:

```bash
sudo capsh --caps="cap_perfmon,cap_setgid,cap_setuid,cap_setpcap+eip" \
  --user=`whoami` \
  --addamb='cap_perfmon' \
  -- -c 'php -d extension=modules/perfidious.so examples/watch.php --interval 2 --pid 1'
```

**Q:** I get an error like
`perf_event_open() failed for perf::PERF_COUNT_HW_INSTRUCTIONS: Permission denied`

**A:** You may need to adjust `kernel.perf_event_paranoid`, for example:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```

**Q:** I get an error like
`perf_event_open() failed for perf::PERF_COUNT_SW_DUMMY: Operation not permitted`
when running inside of docker.

**A:** You may need to run your docker container with CAP_PERFMON:

```bash
docker run --rm -ti --cap-add CAP_PERFMON
```

If it still doesn't work, and you're running an older release of docker, see
[this issue](https://github.com/docker/cli/issues/3960).

**Q:** I get an error like
`perf_event_open() failed for perf::PERF_COUNT_HW_INSTRUCTIONS: No such file or directory`

**A:** If you are using GitHub Actions, or on some other kind of virtualization,
perf events may not be supported. For GitHub Actions, see
[this issue](https://github.com/actions/runner-images/issues/4974)

**Q:** I'm able to read data, but the counters are all zero.

**A:** Reduce the events in the group or try separate handles. The kernel may be unable to schedule the entire group
when hardware-counter capacity is insufficient. Rare events can also produce zero readings.

Capacity varies by processor. An informal Zen4 check observed a limit of roughly four to six hardware counters;
this is not a portable limit.

**Q:** Building from a git checkout fails with a compiler warning treated as an
error (`-Werror`).

**A:** Add `--enable-compile-warnings=yes` to your existing `./configure` options to keep warnings non-fatal.
Nix-shell builds default to fatal warnings; plain Git checkouts and source-archive builds do not.
[Report unexpected fatal warnings outside Nix](https://github.com/jbboehr/php-perfidious/issues), including the compiler
diagnostic and configure options.

## References

* [Linux perf Wiki](https://perf.wiki.kernel.org/index.php/Main_Page)
* [man perf_events_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)
* [libpfm4 Documentation](https://perfmon2.sourceforge.net/docs_v4.html)
* [HHVM perf-event](https://github.com/facebook/hhvm/blob/master/hphp/util/perf-event.cpp)

## License

php-perfidious is licensed under the **GNU Affero General Public License version 3 with the Romic Exception**:

```text
AGPL-3.0-only WITH romic-exception
```

The Romic Exception permits php-perfidious to be linked or combined with other code without subjecting that other code
to the AGPL merely because of the linking or combination. Modifications to the covered project remain subject to the
Project License, including its source-availability requirements for modified versions made available over a computer
network.

See [LICENSE.md](LICENSE.md) and [docs/LICENSE_EXCEPTION.md](docs/LICENSE_EXCEPTION.md) for the complete terms.

Contributions are accepted under the terms in [CONTRIBUTING.md](CONTRIBUTING.md). Unless a contributor elects the CLA
route, each contribution is offered under `AGPL-3.0-only WITH romic-exception OR Apache-2.0`, at each recipient's
option, while the public project incorporates it under the Project License. The Apache-2.0 alternative applies only to
the contributor-authored portions and does not make the project as a whole available under Apache-2.0.

A contributor may instead elect [the CLA](docs/CLA-v1.md), keeping the contribution publicly under the Project License
while granting the [Project Steward](docs/STEWARD.md) the additional rights specified there.

Alternative commercial licenses may be available from the Project Steward. Contact John Boehr at `jbboehr@gmail.com`.
