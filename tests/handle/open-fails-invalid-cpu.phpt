--TEST--
PerfExt\Handle (open fails with invalid cpu)
--EXTENSIONS--
perf
--FILE--
<?php
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
], cpu: PHP_INT_MAX);
--EXPECTF--
%A Uncaught PerfExt\OverflowException: cpu too large: 9223372036854775807 > %d %A