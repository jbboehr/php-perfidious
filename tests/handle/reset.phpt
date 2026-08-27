--TEST--
Perfidious\Handle::reset()
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
var_dump(get_class($rv->reset()));
--EXPECTF--
string(%d) "Perfidious\Handle"