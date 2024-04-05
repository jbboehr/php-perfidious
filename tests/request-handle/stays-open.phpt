--TEST--
PerfExt\request_handle() - stays open
--EXTENSIONS--
perf
--INI--
perf.request.enable=1
perf.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
(function () {
    $handle = PerfExt\request_handle();
    var_dump(get_class($handle));
    var_dump($handle->readArray());
})();
(function () {
    $handle = PerfExt\request_handle();
    var_dump(get_class($handle));
    var_dump($handle->readArray());
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