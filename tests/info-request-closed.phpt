--TEST--
phpinfo per-request stats - closed fd
--EXTENSIONS--
perfidious
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u,perf::PERF_COUNT_SW_PAGE_FAULTS:u,perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u
--FILE--
<?php
$handle = Perfidious\request_handle();
fclose($handle->rawStream());
phpinfo(INFO_MODULES);
--EXPECTF--
%A READ ERROR %A
%A Uncaught Perfidious\IOException: failed to read: Bad file descriptor %A
%A Uncaught Perfidious\IOException: ioctl failed: Bad file descriptor %A