--TEST--
Perfidious\Handle::reset()
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
var_dump(get_class($rv->reset()));
--EXPECTF--
string(%d) "Perfidious\Handle"