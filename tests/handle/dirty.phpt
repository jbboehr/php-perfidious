--TEST--
Perfidious\Handle::rawStream() - do dirty things
--EXTENSIONS--
perfidious
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$stream = $handle->rawStream();
var_dump(strlen(fread($stream, 32)));
fclose($stream);
var_dump($handle->read());
--EXPECTF--
int(32)
%A Uncaught Perfidious\IOException: failed to read: Bad file descriptor %A
%A Uncaught Perfidious\IOException: close failed: Bad file descriptor %A
