--TEST--
Perfidious\Handle (open fails with missing cap, debug)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (get_current_user() === 'root' || str_contains(get_current_user(), 'nixbld')) die("skip: would fail as root"); ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
// Note: this test will fail if run with CAP_PERFMON (e.g. as root)
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
], pid: 1);
--EXPECTF--
%A Uncaught Perfidious\IOException: perf_event_open() failed for perf::PERF_COUNT_SW_DUMMY: Permission denied %A