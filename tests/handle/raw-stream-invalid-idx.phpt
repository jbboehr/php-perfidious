--TEST--
Perfidious\Handle::rawStream() - invalid idx
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$handle->enable();
$stream = $handle->rawStream(PHP_INT_MAX);
var_dump($stream);
--EXPECT--
NULL