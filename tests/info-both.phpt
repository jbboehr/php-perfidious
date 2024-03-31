--TEST--
info
--EXTENSIONS--
perf
--INI--
perf.global.enable=1
perf.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
perf.request.enable=1
perf.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%A
perf
%A
Version => %A
Released => %A
Authors => %A
%A
Directive => Local Value => Master Value
perf.global.enable => 1 => 1
perf.global.metrics => perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES => perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
perf.request.enable => 1 => 1
perf.request.metrics => perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES => perf::PERF_COUNT_SW_CPU_CLOCK,perf::PERF_COUNT_SW_PAGE_FAULTS,perf::PERF_COUNT_SW_CONTEXT_SWITCHES
%A
%sGlobal Metrics%s
perf::PERF_COUNT_SW_CPU_CLOCK => %d
perf::PERF_COUNT_SW_PAGE_FAULTS => %d
perf::PERF_COUNT_SW_CONTEXT_SWITCHES => %d
%A
%sRequest Metrics%s
perf::PERF_COUNT_SW_CPU_CLOCK => %d
perf::PERF_COUNT_SW_PAGE_FAULTS => %d
perf::PERF_COUNT_SW_CONTEXT_SWITCHES => %d
%A
