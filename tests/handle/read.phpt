--TEST--
PerfExt\Handle::read()
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
$value = $rv->read();
var_dump($value);
--EXPECTF--
object(PerfExt\ReadResult)#%d (4) {
  ["timeEnabled"]=>
  int(%d)
  ["timeRunning"]=>
  int(%d)
  ["values"]=>
  array(1) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK"]=>
    int(%d)
  }
  ["lostValues"]=>
  %A
}