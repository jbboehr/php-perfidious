--TEST--
PerfExt\Handle (open fails with invalid pid)
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
], pid: PHP_INT_MAX);
--EXPECTF--
%A Uncaught PerfExt\OverflowException: pid too large: 9223372036854775807 > 2147483647 %A