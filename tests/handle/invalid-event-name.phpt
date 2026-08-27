--TEST--
Perfidious\Handle::open() - invalid event name
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$rv = Perfidious\open([
    "blahblahblah",
]);
--EXPECTF--
%A Uncaught Perfidious\PmuEventNotFoundException: failed to get libpfm event encoding for blahblahblah: event not found %A