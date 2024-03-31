--TEST--
PerfExt\VERSION
--EXTENSIONS--
perf
--INI--
perf.enable=1
perf.metrics=PERF_FLAG_SW_CPU_CLOCK,PERF_FLAG_SW_PAGE_FAULTS
--FILE--
<?php
var_dump(PerfExt\VERSION);
--EXPECTF--
string(%d) "%d.%d.%d"