--TEST--
Perfidious\global_handle() - disabled
--EXTENSIONS--
perf
--INI--
perf.global.enable=0
perf.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
$handle = Perfidious\global_handle();
var_dump($handle);
--EXPECT--
NULL