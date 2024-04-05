--TEST--
Perfidious\Handle::enable()
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([
    "blahblahblah",
]);
--EXPECTF--
%A Uncaught Perfidious\PmuEventNotFoundException: failed to get libpfm event encoding for blahblahblah: event not found %A