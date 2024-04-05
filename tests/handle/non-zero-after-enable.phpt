--TEST--
PerfExt\Handle (non-zero after enable)
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
$value = $rv->readArray();
var_dump($value > 0);
--EXPECT--
bool(true)