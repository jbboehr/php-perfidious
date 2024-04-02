--TEST--
PerfExt\Handle (open fails with missing cap)
--EXTENSIONS--
perf
--SKIPIF--
<?php if (get_current_user() === 'root') die("Skipped: would fail as root"); ?>
--FILE--
<?php
// Note: this test will fail if run with CAP_PERFMON (e.g. as root)
$rv = PerfExt\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK",
], pid: 1);
--EXPECTF--
%A Uncaught PerfExt\IOException: pid greater than zero and CAP_PERFMON %A