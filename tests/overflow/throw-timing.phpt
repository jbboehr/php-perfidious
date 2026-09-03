--TEST--
overflow in Handle timing fields throws
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--FILE--
<?php
require __DIR__ . '/../inject-scaling-read.inc';

$handle = Perfidious\open([]);
$uint64Max = str_repeat("\xff", 8);
$one = pack('Q', 1);

foreach (
    [
        'timeEnabled' => [$uint64Max, $one],
        'timeRunning' => [$one, $uint64Max],
    ] as $field => [$timeEnabled, $timeRunning]
) {
    $stream = injectScalingRead($handle, $timeEnabled, $timeRunning);

    try {
        $handle->read();
        echo "$field unexpectedly succeeded\n";
    } catch (Perfidious\OverflowException $e) {
        echo "$field: ", $e->getMessage(), "\n";
    } finally {
        fclose($stream);
    }
}
--EXPECTF--
timeEnabled: value too large: %d > %d
timeRunning: value too large: %d > %d
