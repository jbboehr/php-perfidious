--TEST--
Perfidious\Handle (non-zero after enable)
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$rv->enable();
for ($i = 0; $i < 100; $i++) {
    usleep(1);
}
$value = $rv->readArray();
var_dump($value > 0);
--EXPECT--
bool(true)