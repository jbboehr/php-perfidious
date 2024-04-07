--TEST--
Perfidious\Handle::read()
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$rv->enable();
for ($i = 0; $i < 100; $i++) {
    usleep(1);
}
$rv->debugCorruptMetricIds();
$value = $rv->read();
var_dump($value);
--EXPECTF--
%A Uncaught DomainException: ID mismatch: %d != %d %A