--TEST--
Perfidious\Handle::read()
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
$value = $rv->read();
var_dump($value);
--EXPECTF--
object(Perfidious\ReadResult)#%d (3) {
  ["timeEnabled"]=>
  int(%d)
  ["timeRunning"]=>
  int(%d)
  ["values"]=>
  array(1) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
    int(%d)
  }
}