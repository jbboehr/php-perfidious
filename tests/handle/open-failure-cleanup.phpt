--TEST--
Failed handle construction releases partial event groups and permits a subsequent open
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--FILE--
<?php

$events = ['perf::PERF_COUNT_SW_CPU_CLOCK:u', 'perf::PERF_COUNT_SW_PAGE_FAULTS:u'];
$baseline = count(scandir('/proc/self/fd'));
for ($i = 0; $i < 3; $i++) {
    try {
        Perfidious\open([...$events, 'perfidious-nonexistent-event']);
        throw new RuntimeException('Invalid event unexpectedly opened');
    } catch (Perfidious\PmuEventNotFoundException) {
        var_dump(count(scandir('/proc/self/fd')) === $baseline);
    }
}
$handle = Perfidious\open($events);
var_dump(count(scandir('/proc/self/fd')) === $baseline + 3);
var_dump(count($handle->readArray()) === 2);
unset($handle);
var_dump(count(scandir('/proc/self/fd')) === $baseline);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
