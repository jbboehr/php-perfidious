--TEST--
phpinfo global and per-request stats
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
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
perfidious.overflow_mode => 0 => 0
perfidious.request.enable => 1 => 1
perfidious.request.metrics => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
%A
%sGlobal Metrics%s
Event => Counter => Scaled => % Running
perf::PERF_COUNT_SW_CPU_CLOCK:u => %d => %d => %d%
perf::PERF_COUNT_SW_PAGE_FAULTS:u => %d => %d => %d%
perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => %d => %d => %d%
%A
%sRequest Metrics%s
Event => Counter => Scaled => % Running
perf::PERF_COUNT_SW_CPU_CLOCK:u => %d => %d => %d%
perf::PERF_COUNT_SW_PAGE_FAULTS:u => %d => %d => %d%
perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => %d => %d => %d%
%A
