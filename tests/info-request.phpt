--TEST--
phpinfo per-request stats
--EXTENSIONS--
perfidious
--INI--
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
perfidious.global.enable => 0 => 0
perfidious.global.metrics => %s => %s
perfidious.request.enable => 1 => 1
perfidious.request.metrics => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
%A
%sRequest Metrics%s
Event => Counter => Scaled => % Running
perf::PERF_COUNT_SW_CPU_CLOCK:u => %d => %d => %d%
perf::PERF_COUNT_SW_PAGE_FAULTS:u => %d => %d => %d%
perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u => %d => %d => %d%
%A
