--TEST--
Perfidious\Handle::debugCloseFd() exercised directly against read()/readArray()/enable()
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$handle->debugCloseFd();

try {
    $handle->read();
} catch (Perfidious\IOException $e) {
    echo $e->getMessage(), "\n";
}

try {
    $handle->readArray();
} catch (Perfidious\IOException $e) {
    echo $e->getMessage(), "\n";
}

try {
    $handle->enable();
} catch (Perfidious\IOException $e) {
    echo $e->getMessage(), "\n";
}
--EXPECTF--
failed to read: Bad file descriptor
failed to read: Bad file descriptor
ioctl failed: Bad file descriptor
%A Uncaught Perfidious\IOException: close failed: Bad file descriptor %A
