--TEST--
phpinfo global stats - closed fd
--EXTENSIONS--
perfidious
--INI--
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
$handle = Perfidious\global_handle();
fclose($handle->rawStream());
phpinfo(INFO_MODULES);
// looks like the second error is happening during mshutdown and doesn't get printed?
--EXPECTF--
%A READ ERROR %A
%A Uncaught Perfidious\IOException: failed to read: Bad file descriptor %A