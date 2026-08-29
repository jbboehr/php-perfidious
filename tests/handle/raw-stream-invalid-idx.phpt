--TEST--
Perfidious\Handle::rawStream() - invalid idx
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$handle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$handle->enable();

foreach ([2, PHP_INT_MAX] as $idx) {
    try {
        $handle->rawStream($idx);
        echo "accepted invalid index\n";
    } catch (ValueError) {
        echo "invalid index $idx\n";
    }
}

$stream = $handle->rawStream(1);
var_dump(strlen(fread($stream, 32)));
fclose($stream);
--EXPECTF--
invalid index 2
invalid index %d
int(32)
