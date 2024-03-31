--TEST--
PerfExt\Handle::disable
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
var_dump(get_class($rv->disable()));
--EXPECT--
string(14) "PerfExt\Handle"