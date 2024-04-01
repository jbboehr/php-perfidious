--TEST--
PerfExt\global_handle() - stays open
--EXTENSIONS--
perf
--INI--
perf.global.enable=1
perf.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
(function () {
    $handle = PerfExt\global_handle();
    var_dump(get_class($handle));
    var_dump($handle->read());
})();
(function () {
    $handle = PerfExt\global_handle();
    var_dump(get_class($handle));
    var_dump($handle->read());
})();
--EXPECTF--
string(14) "PerfExt\Handle"
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES"]=>
  int(%d)
}
string(14) "PerfExt\Handle"
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES"]=>
  int(%d)
}