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
--EXPECT--
bool(true)
bool(true)
