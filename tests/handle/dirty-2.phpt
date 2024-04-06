--TEST--
Perfidious\Handle::rawStream() - do dirty things part 2
--EXTENSIONS--
perfidious
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    "perf::PERF_COUNT_SW_PAGE_FAULTS:u",
    "perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u",
]);
$stream = $handle->rawStream(2);
var_dump(strlen(fread($stream, 32)));
fclose($stream);
var_dump($handle->read());
--EXPECTF--
int(32)
%A Uncaught Perfidious\IOException: failed to read: Illegal seek %A
%A Uncaught Perfidious\IOException: close failed: Bad file descriptor %A
