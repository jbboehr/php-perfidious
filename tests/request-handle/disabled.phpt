--TEST--
PerfExt\request_handle() - disabled
--EXTENSIONS--
perf
--INI--
perf.request.enable=0
perf.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
$handle = PerfExt\request_handle();
var_dump($handle);
--EXPECT--
NULL