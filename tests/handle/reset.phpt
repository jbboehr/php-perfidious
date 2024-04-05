--TEST--
Perfidious\Handle::reset()
--EXTENSIONS--
perf
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
var_dump(get_class($rv->reset()));
--EXPECTF--
string(%d) "Perfidious\Handle"