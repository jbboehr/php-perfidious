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
try {
    var_dump($handle->enable());
} catch (\Throwable $e) {
    var_dump($e->getMessage());
}
try {
    var_dump($handle->disable());
} catch (\Throwable $e) {
    var_dump($e->getMessage());
}
try {
    var_dump($handle->reset());
} catch (\Throwable $e) {
    var_dump($e->getMessage());
}
--EXPECTF--
int(32)
string(%d) "ioctl failed: Bad file descriptor"
string(%d) "ioctl failed: Bad file descriptor"
string(%d) "ioctl failed: Bad file descriptor"
%A Uncaught Perfidious\IOException: close failed: Bad file descriptor %A
