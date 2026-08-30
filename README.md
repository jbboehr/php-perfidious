
# php-perfidious

[![ci](https://github.com/jbboehr/php-perfidious/actions/workflows/ci.yml/badge.svg)](https://github.com/jbboehr/php-perfidious/actions/workflows/ci.yml)
[![Codecov](https://codecov.io/gh/jbboehr/php-perfidious/graph/badge.svg?token=DSLDXIWHC5)](https://codecov.io/gh/jbboehr/php-perfidious)
[![Coveralls](https://coveralls.io/repos/github/jbboehr/php-perfidious/badge.svg?branch=master)](https://coveralls.io/github/jbboehr/php-perfidious?branch=master)
[![License: AGPL v3+](https://img.shields.io/badge/License-AGPL_v3%2b-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
![Language](https://img.shields.io/github/languages/top/jbboehr/php-perfidious)
![Tag](https://img.shields.io/github/v/tag/jbboehr/php-perfidious)
![stability-experimental](https://img.shields.io/badge/stability-experimental-orange.svg)

This extension provides access to the performance monitoring *counters* exposed
by Linux `perf_events`, plus experimental low-level Windows and macOS APIs.

## Requirements

* PHP 8.1 - 8.5
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
echo extension=perfidious.so | tee -a /path/to/your/php.ini
```

Finally, *restart the web server*.

## Usage

See also the [`examples`](./examples) directory and the [`stub`](./perfidious.stub.php).

### Linux perf_events

For example, you can programmatically open and access the counters.

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

Or you can configure a global or per-request handle:

```php
// with the following INI settings:
// perfidious.request.enable=1
// perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
var_dump(Perfidious\request_handle()?->read());
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
| `perfidious.global.enable` | `0` | `PHP_INI_SYSTEM` | Set to `1` to enable the global handle. This handle is kept open between requests. You can read from this handle via e.g. `var_dump(Perfidious\global_handle()?->read());`. |
| `perfidious.global.metrics` | `perf::PERF_COUNT_HW_CPU_CYCLES:u`, `perf::PERF_COUNT_HW_INSTRUCTIONS:u` | `PHP_INI_SYSTEM` | The metrics to monitor with the global handle. |
| `perfidious.request.enable` | `0` | `PHP_INI_SYSTEM` | Set to `1` to enable the per-request handle. This handle is kept open between requests, but reset before and after. You can read from this handle via e.g. `var_dump(Perfidious\request_handle()?->read());` |
| `perfidious.request.metrics` | `perf::PERF_COUNT_HW_CPU_CYCLES:u`, `perf::PERF_COUNT_HW_INSTRUCTIONS:u` | `PHP_INI_SYSTEM` | The metrics to monitor with the request handle. |

`global.enable` and `request.enable` only really do something useful under a
persistent-worker SAPI like php-fpm: the global handle is opened once and
never reset, so it accumulates across every request the worker ever
handles; the request handle is reset at the start and end of each request,
so it reflects just that one request. Under the CLI SAPI, every invocation
is its own process with exactly one "request", so the two behave
identically there.

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

**A:** This may happen for a few reasons:

1. If you are monitoring several hardware events (e.g.
`perf::PERF_COUNT_HW_INSTRUCTIONS`), the PMU may not have enough capacity to
handle all of them. The limit appears to be per physical CPU core. In testing
on my Zen4 CPU, it appeared that the maximum hardware counters was around 4-6.
If you have any more information on how to tell how many "slots" are available,
please let me know.

2. If, for some reason, the kernel is unable to schedule all events in the
group, it will not schedule any of them. Try removing events until you get
some non-zero data, or opening separate handles. Note also that some events
may be low-frequency.

**Q:** Building from a git checkout fails with a compiler warning treated as an
error (`-Werror`).

**A:** Building inside the project's own `nix develop` shell always treats
warnings as errors by design, so we catch them during development. A plain
`git clone` + `phpize && ./configure` build, and PECL/release-tarball installs,
default to non-fatal warnings instead - if you hit this outside the nix
devShell, please [file an issue](https://github.com/jbboehr/php-perfidious/issues),
since it likely means a warning that's fine on our compilers isn't on yours.
You can also pass `--enable-compile-warnings=yes` explicitly to `./configure`
to disable it yourself.

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
