--TEST--
Perfidious\Handle::rawStream() - do dirty things
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$stream = $handle->rawStream();
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
  array(1) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
    int(%d)
  }
}
