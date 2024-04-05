--TEST--
Perfidious\global_handle() - disabled
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=0
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
$handle = Perfidious\global_handle();
var_dump($handle);
--EXPECT--
NULL