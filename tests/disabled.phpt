--TEST--
disabled
--EXTENSIONS--
perf
--INI--
perf.enable=0
perf.metrics=PERF_FLAG_HW_CPU_CYCLES,PERF_FLAG_HW_INSTRUCTIONS
--FILE--
<?php
var_dump(PerfExt\perf_stat());
--EXPECT--
array(0) {
}