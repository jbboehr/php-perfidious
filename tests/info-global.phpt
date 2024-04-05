--TEST--
info
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%A
perfidious
%A
Version => %A
Released => %A
Authors => %A
%A
Directive => Local Value => Master Value
perfidious.global.enable => 1 => 1
perfidious.global.metrics => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
perfidious.request.enable => 0 => 0
perfidious.request.metrics => %s => %s
%A
%sGlobal Metrics%s
perf::PERF_COUNT_SW_CPU_CLOCK:u => %d
perf::PERF_COUNT_SW_PAGE_FAULTS:u => %d
perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => %d
%A
