--TEST--
overflow (throw)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=0
--FILE--
<?php
try {
    var_dump(Perfidious\debug_uint64_overflow());
} catch (Perfidious\OverflowException $e) {
    var_dump($e->getMessage());
}
try {
    var_dump(Perfidious\debug_uint64_overflow(Perfidious\OVERFLOW_THROW));
} catch (Perfidious\OverflowException $e) {
    var_dump($e->getMessage());
}
--EXPECTF--
string(%d) "value too large: %d > %d"
string(%d) "value too large: %d > %d"