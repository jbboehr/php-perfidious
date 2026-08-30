--TEST--
overflow (wrap)
--EXTENSIONS--
perfidious
gmp
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=3
--FILE--
<?php
$uint64_max = gmp_init(Perfidious\UINT64_MAX);
$wrapped = gmp_mod($uint64_max, gmp_init(PHP_INT_MAX));
var_dump(gmp_strval($wrapped) === (string) Perfidious\debug_uint64_overflow());
var_dump(gmp_strval($wrapped) === (string) Perfidious\debug_uint64_overflow(Perfidious\OVERFLOW_WRAP));

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
$result = $handle->read();

var_dump(gmp_strval($wrapped) === (string) $result->timeEnabled);
var_dump(gmp_strval($wrapped) === (string) $result->timeRunning);
var_dump(gmp_strval($wrapped) === (string) $result->values['perf::PERF_COUNT_SW_CPU_CLOCK:u']);
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
