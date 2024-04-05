--TEST--
PerfExt\Handle (open fails with non-string event)
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([1]);
--EXPECTF--
%A Uncaught TypeError: All event names must be strings %A