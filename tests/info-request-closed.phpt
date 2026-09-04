--TEST--
phpinfo per-request stats - closed fd
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
$handle = Perfidious\request_handle();
$handle->debugCloseFd();
phpinfo(INFO_MODULES);
--EXPECTF--
%A READ ERROR %A
%A Uncaught Perfidious\IOException: failed to read: Bad file descriptor %A
