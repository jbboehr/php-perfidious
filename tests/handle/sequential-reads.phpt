--TEST--
Perfidious\Handle (sequential reads)
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
$rv->disable();
$orig = $rv->readArray();
$same = 0;
for ($i = 0; $i < 1000; $i++) {
    $same += (int) ($orig == $rv->readArray());
}
var_dump($same);
--EXPECT--
int(1000)