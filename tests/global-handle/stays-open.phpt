--TEST--
Perfidious\global_handle() - stays open
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
(function () {
    $handle = Perfidious\global_handle();
    var_dump(get_class($handle));
    var_dump($handle->readArray());
})();
(function () {
    $handle = Perfidious\global_handle();
    var_dump(get_class($handle));
    var_dump($handle->readArray());
})();
--EXPECTF--
string(%d) "Perfidious\Handle"
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES"]=>
  int(%d)
}
string(%d%d) "Perfidious\Handle"
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES"]=>
  int(%d)
}