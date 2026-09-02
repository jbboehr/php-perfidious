--TEST--
Perfidious\Handle (open fails with missing cap)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (Perfidious\DEBUG) die("skip: must not be compiled in debug mode"); ?>
<?php
require __DIR__ . '/../linux-capabilities.inc';

$status = @file_get_contents('/proc/self/status');
if ($status === false) {
    die("skip: cannot read effective capabilities");
}

// CAP_PERFMON is capability number 38 in the Linux UAPI.
$hasCapPerfmon = perfidious_linux_status_has_effective_capability($status, 38);
if ($hasCapPerfmon === null) {
    die("skip: cannot determine effective capabilities");
}
if ($hasCapPerfmon) {
    die("skip: CAP_PERFMON is effective");
}
?>
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
], pid: 1);
--EXPECTF--
%A Uncaught Perfidious\IOException: pid greater than zero and CAP_PERFMON %A
