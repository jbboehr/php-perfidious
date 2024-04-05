--TEST--
Perfidious\request_handle() - enabled
--EXTENSIONS--
perfidious
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
$handle = Perfidious\request_handle();
var_dump(get_class($handle));
var_dump($handle->readArray());
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