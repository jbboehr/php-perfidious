--TEST--
basic
--EXTENSIONS--
perf
--INI--
perf.enable=1
perf.metrics=PERF_FLAG_HW_CPU_CYCLES,PERF_FLAG_HW_INSTRUCTIONS
--FILE--
<?php
var_dump(PerfExt\perf_stat());
--EXPECTF--
array(2) {
  ["PERF_FLAG_HW_CPU_CYCLES"]=>
  int(%d)
  ["PERF_FLAG_HW_INSTRUCTIONS"]=>
  int(%d)
}