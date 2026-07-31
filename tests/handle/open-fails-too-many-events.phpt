--TEST--
Perfidious\Handle (open fails with too many event names)
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open(array_fill(0, 1001, "perf::PERF_COUNT_SW_CPU_CLOCK:u"));
--EXPECTF--
%A Uncaught Perfidious\OverflowException: too many event names: 1001 > 1000 %A
