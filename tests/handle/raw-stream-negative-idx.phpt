--TEST--
Perfidious\Handle::rawStream() - negative idx
--EXTENSIONS--
perfidious
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$handle->enable();
$stream = $handle->rawStream(-1);
var_dump($stream);
--EXPECT--
NULL
