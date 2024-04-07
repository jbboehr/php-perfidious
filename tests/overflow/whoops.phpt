--TEST--
overflow (whoops)
--EXTENSIONS--
perfidious
gmp
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=3
--FILE--
<?php
Perfidious\debug_uint64_overflow(PHP_INT_MAX);
--EXPECTF--
%A Uncaught TypeError: Overflow mode out-of-range %A