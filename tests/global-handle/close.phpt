--TEST--
Perfidious\Handle::close() detaches a global-handle wrapper without closing shared counters
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php

use Perfidious\IOException;

$handle = Perfidious\global_handle();
$handle->close();
$handle->close();

try {
    $handle->read();
    echo "detached wrapper unexpectedly remained usable\n";
} catch (IOException) {
    echo "global wrapper detached\n";
}

$replacement = Perfidious\global_handle();
var_dump(array_keys($replacement->readArray()));
$replacement->close();

$secondReplacement = Perfidious\global_handle();
var_dump(array_keys($secondReplacement->readArray()));
?>
--EXPECT--
global wrapper detached
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
