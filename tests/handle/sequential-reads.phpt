--TEST--
PerfExt\Handle (sequential reads)
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
]);
$rv->enable();
for ($i = 0; $i < 100; $i++) {
    usleep(1);
}
$rv->disable();
$orig = $rv->read();
$same = 0;
for ($i = 0; $i < 1000; $i++) {
    $same += (int) ($orig == $rv->read());
}
var_dump($same);
--EXPECT--
int(1000)