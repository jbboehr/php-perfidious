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
--EXPECTF--
bool(true)
bool(true)