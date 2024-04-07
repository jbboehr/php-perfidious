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
--EXPECTF--
Warning: Perfidious\debug_uint64_overflow(): value too large: %d > %d in %A
NULL

Warning: Perfidious\debug_uint64_overflow(): value too large: %d > %d in %A
NULL