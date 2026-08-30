--TEST--
overflow (saturate)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=2
--FILE--
<?php
var_dump(PHP_INT_MAX === Perfidious\debug_uint64_overflow());
var_dump(PHP_INT_MAX === Perfidious\debug_uint64_overflow(Perfidious\OVERFLOW_SATURATE));

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
$result = $handle->read();

var_dump(PHP_INT_MAX === $result->timeEnabled);
var_dump(PHP_INT_MAX === $result->timeRunning);
var_dump(PHP_INT_MAX === $result->values['perf::PERF_COUNT_SW_CPU_CLOCK:u']);
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
