--TEST--
Perfidious\Handle::enable()
--EXTENSIONS--
perf
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
var_dump(get_class($rv->enable()));
--EXPECTF--
string(%d) "Perfidious\Handle"