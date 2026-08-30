--TEST--
overflow warning policy applies to Handle timing fields
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=1
--FILE--
<?php
// With no requested metrics, the synthetic dummy metric is skipped and the
// first overflowing value exposed by read() is timeEnabled.
$handle = Perfidious\open([]);
$handle->debugInjectOverflowRead();
var_dump($handle->read());
--EXPECTF--
Warning: Perfidious\Handle::read(): value too large: %d > %d in %A
NULL
