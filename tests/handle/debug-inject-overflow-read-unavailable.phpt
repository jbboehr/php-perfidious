--TEST--
Perfidious\Handle::debugInjectOverflowRead() is unavailable in release builds
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (Perfidious\DEBUG) die("skip: must not be compiled in debug mode"); ?>
--FILE--
<?php
var_dump(method_exists(Perfidious\Handle::class, 'debugInjectOverflowRead'));
var_dump(method_exists(Perfidious\Handle::class, 'debugInjectScalingRead'));
--EXPECT--
bool(false)
bool(false)
