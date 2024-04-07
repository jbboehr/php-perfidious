--TEST--
Perfidious\Handle::rawStream()
--EXTENSIONS--
perfidious
--FILE--
<?php
(function () {
    $handle = Perfidious\open([
        "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    ]);
    $handle->enable();
    $stream = $handle->rawStream();
    var_dump(strlen(fread($stream, 32)));
    var_dump($handle->read());
})();
gc_collect_cycles(); 
--EXPECTF--
int(32)
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