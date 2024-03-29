--TEST--
basic
--EXTENSIONS--
perf
--INI--
perf.enable=1
perf.metrics=PERF_FLAG_SW_CPU_CLOCK,PERF_FLAG_SW_PAGE_FAULTS
--FILE--
<?php
var_dump(PerfExt\perf_stat());
--EXPECTF--
array(2) {
  ["PERF_FLAG_SW_CPU_CLOCK"]=>
  int(%d)
  ["PERF_FLAG_SW_PAGE_FAULTS"]=>
  int(%d)
}