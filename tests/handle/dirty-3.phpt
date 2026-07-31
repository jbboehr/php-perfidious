--TEST--
Perfidious\Handle::rawStream() - do dirty things part 3
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
// rawStream() hands out a dup()'d fd, so closing it must not break the handle
var_dump(get_class($handle->enable()));
var_dump(get_class($handle->disable()));
var_dump(get_class($handle->reset()));
--EXPECTF--
int(32)
string(%d) "Perfidious\Handle"
string(%d) "Perfidious\Handle"
string(%d) "Perfidious\Handle"
