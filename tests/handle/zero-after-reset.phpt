--TEST--
PerfExt\Handle (zero after reset)
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
$rv->reset();
var_dump($rv->readArray());
--EXPECT--
array(1) {
  ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
  int(0)
}