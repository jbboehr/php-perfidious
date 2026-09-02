--TEST--
Perfidious\Handle (open fails with missing cap, debug)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
<?php
try {
    $probe = Perfidious\open([
        "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    ], pid: 1);
    $probe->close();
    die("skip: profiling pid 1 is permitted");
} catch (Perfidious\IOException $e) {
    if ($e->getCode() !== 13) {
        die("skip: profiling pid 1 did not fail with EACCES");
    }
}
?>
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
], pid: 1);
--EXPECTF--
%A Uncaught Perfidious\IOException: perf_event_open() failed for perf::PERF_COUNT_SW_DUMMY: Permission denied %A
