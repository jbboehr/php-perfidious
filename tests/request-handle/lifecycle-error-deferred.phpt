--TEST--
Perfidious request-handle lifecycle errors are deferred until the handle is requested
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php
$handle = Perfidious\request_handle();
$handle->debugCloseFd();

echo "request body completed\n";
?>
--EXPECT--
request body completed
