--TEST--
PerfExt\Handle::enable()
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "blahblahblah",
]);
--EXPECTF--
%A Uncaught PerfExt\PmuEventNotFoundException: failed to get libpfm event encoding for blahblahblah: event not found %A