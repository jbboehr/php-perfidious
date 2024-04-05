--TEST--
Perfidious\request_handle() - enabled
--EXTENSIONS--
perfidious
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
$handle = Perfidious\request_handle();
var_dump(get_class($handle));
var_dump($handle->readArray());
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