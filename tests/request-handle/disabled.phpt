--TEST--
Perfidious\request_handle() - disabled
--EXTENSIONS--
perfidious
--INI--
perfidious.request.enable=0
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$handle = Perfidious\request_handle();
var_dump($handle);
--EXPECT--
NULL
