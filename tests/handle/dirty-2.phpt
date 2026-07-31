--TEST--
Perfidious\Handle::rawStream() - do dirty things part 2
--EXTENSIONS--
perfidious
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    "perf::PERF_COUNT_SW_PAGE_FAULTS:u",
    "perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u",
]);
$stream = $handle->rawStream(2);
var_dump(strlen(fread($stream, 32)));
fclose($stream);
// rawStream() hands out a dup()'d fd, so closing it must not break the handle
var_dump($handle->read());
--EXPECTF--
int(32)
object(Perfidious\ReadResult)#%d (3) {
  ["timeEnabled"]=>
  int(%d)
  ["timeRunning"]=>
  int(%d)
  ["values"]=>
  array(3) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
    int(%d)
    ["perf::PERF_COUNT_SW_PAGE_FAULTS:u"]=>
    int(%d)
    ["perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u"]=>
    int(%d)
  }
}
