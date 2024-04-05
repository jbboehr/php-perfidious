--TEST--
Perfidious\Handle::disable()
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
var_dump(get_class($rv->disable()));
--EXPECTF--
string(%d) "Perfidious\Handle"