--TEST--
Perfidious Windows thread profiling preserves hardware counter indices
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-windows-only.inc';

try {
    $profile = Perfidious\Windows\enable_current_thread_profiling(hardwareCounterMask: 8);
    $profile->read();
    $profile->close();
} catch (Perfidious\IOException $exception) {
    die('skip: Windows hardware counter profiling is unavailable');
}
?>
--FILE--
<?php

$profile = Perfidious\Windows\enable_current_thread_profiling(hardwareCounterMask: 8);
$data = $profile->read();
$counter = $data->hardwareCounters[3];

var_dump(
    $data->hardwareCounterCount <= 1 &&
    array_keys($data->hardwareCounters) === [3] &&
    $counter instanceof Perfidious\Windows\HardwareCounterSnapshot &&
    array_keys(get_object_vars($counter)) === ['index', 'type', 'value'] &&
    $counter->index === 3
);

$profile->close();
--EXPECT--
bool(true)
