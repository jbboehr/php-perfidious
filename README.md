
# php-perf

[![ci](https://github.com/jbboehr/php-perf/actions/workflows/ci.yml/badge.svg)](https://github.com/jbboehr/php-perf/actions/workflows/ci.yml)
[![Coverage Status](https://coveralls.io/repos/github/jbboehr/php-perf/badge.svg?branch=master)](https://coveralls.io/github/jbboehr/php-perf?branch=master)
[![License: AGPL v3+](https://img.shields.io/badge/License-AGPL_v3%2b-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
![stability-experimental](https://img.shields.io/badge/stability-experimental-orange.svg)

This extension provides access to the performance monitoring *counters* exposed
by the Linux `perf_events` kernel API.

## Requirements

As we are calling Linux kernel APIs, this extension will only work on **Linux**.

* PHP 8.1 - 8.3
* libcap
* libpfm4

## Installation

### Source

You will need a few packages, including libcap and libpfm4. On Ubuntu and
Debian, this should be:

```bash
apt install build-essential libcap-dev libpfm4-dev php-dev
```

Now clone the repo and compile the extension:

```bash
git clone https://github.com/jbboehr/php-perf.git
cd php-perf
phpize
./configure
make
make test
sudo make install
````

Add the extension to your *php.ini*:

```ini
echo extension=perf.so | tee -a /path/to/your/php.ini
```

Finally, *restart the web server*.

## Usage

See also the [`examples`](./examples) directory and the [`stub`](./perf.stub.php).

For example, you can programmatically open and access the counters.

```php
$handle = PerfExt\open(["perf::PERF_COUNT_SW_CPU_CLOCK"]);
$handle->enable();

for ($i = 0; $i < 3; $i++) {
    var_dump($handle->read());
    sleep(1);
}
```

```text
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(3190)
}
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(51270)
}
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(86560)
}
```

Or you can configure a global or per-request handle:

```php
// with the following INI settings:
// perf.request.enable=1
// perf.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
var_dump(PerfExt\request_handle()?->read());
```

```text
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(120880)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS"]=>
  int(64)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES"]=>
  int(0)
}
```

## Events

We use the libpfm4 event name encoding to open events. To see a list of all events,
execute [examples/all-events.php](examples/all-events.php) with the extension loaded
or see the [libpfm4 documentation](https://perfmon2.sourceforge.net/docs_v4.html).
Some notable generic perf events are:

* `perf::PERF_COUNT_HW_CPU_CYCLES`
* `perf::PERF_COUNT_HW_INSTRUCTIONS`
* `perf::PERF_COUNT_SW_PAGE_FAULTS`
* `perf::PERF_COUNT_SW_CONTEXT_SWITCHES`

## Troubleshooting

**Q:** I get an error `pid greater than zero and CAP_PERFMON not set`

**A:** You need to grant `CAP_PERFMON` when monitoring a process besides the
current process, for example:

```bash
sudo capsh --caps="cap_perfmon,cap_setgid,cap_setuid,cap_setpcap+eip" \
  --user=`whoami` \
  --addamb='cap_perfmon' \
  -- -c 'php -d extension=modules/perf.so examples/watch.php --interval 2 --pid 1'
```

**Q:** I get an error like
`perf_event_open() failed for perf::PERF_COUNT_HW_INSTRUCTIONS: Permission denied`

**A:** You may need to adjust `kernel.perf_event_paranoid`, for example:

```bash
sudo sysctl -w kernel.perf_event_paranoid=1
```

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

## References

* [Linux perf Wiki](https://perf.wiki.kernel.org/index.php/Main_Page)
* [man perf_events_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)
* [libpfm4 Documentation](https://perfmon2.sourceforge.net/docs_v4.html)
* [HHVM perf-event](https://github.com/facebook/hhvm/blob/master/hphp/util/perf-event.cpp)

## License

This project is licensed under the [AGPLv3.0 or later](LICENSE.md).
