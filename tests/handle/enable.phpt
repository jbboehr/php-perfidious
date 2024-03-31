--TEST--
PerfExt\Handle::enable
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
var_dump(get_class($rv->enable()));
--EXPECT--
string(14) "PerfExt\Handle"