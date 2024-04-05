--TEST--
Perfidious\global_handle() - stays open
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
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
  ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS:u"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u"]=>
  int(%d)
}
string(%d%d) "Perfidious\Handle"
array(3) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_PAGE_FAULTS:u"]=>
  int(%d)
  ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u"]=>
  int(%d)
}