--TEST--
overflow (warn)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=1
--FILE--
<?php
var_dump(Perfidious\debug_uint64_overflow());
var_dump(Perfidious\debug_uint64_overflow(Perfidious\OVERFLOW_WARN));

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
var_dump($handle->read());

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
var_dump($handle->readArray());

var_dump((new ReflectionMethod(Perfidious\Handle::class, 'read'))->getReturnType()->allowsNull());
var_dump((new ReflectionMethod(Perfidious\Handle::class, 'readArray'))->getReturnType()->allowsNull());
--EXPECTF--
Warning: Perfidious\debug_uint64_overflow(): value too large: %d > %d in %A
NULL

Warning: Perfidious\debug_uint64_overflow(): value too large: %d > %d in %A
NULL

Warning: Perfidious\Handle::read(): value too large: %d > %d in %A
NULL

Warning: Perfidious\Handle::readArray(): value too large: %d > %d in %A
NULL
bool(true)
bool(true)
