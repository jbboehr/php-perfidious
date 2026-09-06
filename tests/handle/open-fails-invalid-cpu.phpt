--TEST--
Perfidious\Handle (open fails with invalid cpu)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (PHP_INT_SIZE < 8) die('skip: requires PHP integers wider than native CPU IDs'); ?>
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
], cpu: PHP_INT_MAX);
--EXPECTF--
%A Uncaught Perfidious\OverflowException: cpu too large: 9223372036854775807 > %d %A
