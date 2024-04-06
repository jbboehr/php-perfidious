--TEST--
Perfidious\Handle::open() - invalid event name 2
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    "perf::PERF_COUNT_SW_PAGE_FAULTS:u",
    "perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u",
    "blahblahblah",
]);
--EXPECTF--
%A Uncaught Perfidious\PmuEventNotFoundException: failed to get libpfm event encoding for blahblahblah: event not found %A