--TEST--
Perfidious\Handle (open with zero event names)
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([]);
$rv->enable();
for ($i = 0; $i < 100; $i++) {
    usleep(1);
}
var_dump($rv->read());
var_dump($rv->readArray());
--EXPECTF--
object(Perfidious\ReadResult)#%d (3) {
  ["timeEnabled"]=>
  int(%d)
  ["timeRunning"]=>
  int(%d)
  ["values"]=>
  array(0) {
  }
}
array(0) {
}
