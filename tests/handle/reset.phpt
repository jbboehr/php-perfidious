--TEST--
PerfExt\Handle::reset()
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
var_dump(get_class($rv->reset()));
--EXPECT--
string(14) "PerfExt\Handle"