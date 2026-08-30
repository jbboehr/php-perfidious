--TEST--
Perfidious\Handle::close() detaches a request-handle wrapper without closing shared counters
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php

use Perfidious\IOException;

$handle = Perfidious\request_handle();
$handle->close();
$handle->close();

try {
    $handle->read();
    echo "detached wrapper unexpectedly remained usable\n";
} catch (IOException) {
    echo "request wrapper detached\n";
}

$replacement = Perfidious\request_handle();
var_dump(array_keys($replacement->readArray()));
$replacement->close();

$secondReplacement = Perfidious\request_handle();
var_dump(array_keys($secondReplacement->readArray()));
?>
--EXPECT--
request wrapper detached
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
