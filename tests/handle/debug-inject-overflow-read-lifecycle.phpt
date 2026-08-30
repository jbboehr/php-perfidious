--TEST--
Perfidious\Handle::debugInjectOverflowRead() supports repeated reads without leaking descriptors
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--INI--
perfidious.overflow_mode=2
--FILE--
<?php
function descriptorCount(): int
{
    return count(scandir('/proc/self/fd')) - 2;
}

function check(bool $condition, string $label): void
{
    echo $label, ': ', $condition ? 'ok' : 'failed', "\n";
}

$baseline = descriptorCount();
$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$opened = descriptorCount();

foreach (['read', 'readArray', 'read'] as $method) {
    $handle->debugInjectOverflowRead();
    $result = $handle->$method();

    if ($result instanceof Perfidious\ReadResult) {
        check($result->timeEnabled === PHP_INT_MAX, "$method timeEnabled saturated");
        check($result->timeRunning === PHP_INT_MAX, "$method timeRunning saturated");
        $values = $result->values;
    } else {
        $values = $result;
    }

    check($values['perf::PERF_COUNT_SW_CPU_CLOCK:u'] === PHP_INT_MAX, "$method metric saturated");
    check(descriptorCount() === $opened, "$method descriptor count stable");
}

unset($handle, $result, $values);
gc_collect_cycles();
check(descriptorCount() === $baseline, 'destruction restored descriptor count');

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
$handle->close();

try {
    $handle->debugInjectOverflowRead();
    echo "debug injection unexpectedly accepted a closed handle\n";
} catch (Perfidious\IOException) {
    echo "debug injection rejected a closed handle\n";
}

check(descriptorCount() === $baseline, 'close restored descriptor count');
--EXPECT--
read timeEnabled saturated: ok
read timeRunning saturated: ok
read metric saturated: ok
read descriptor count stable: ok
readArray metric saturated: ok
readArray descriptor count stable: ok
read timeEnabled saturated: ok
read timeRunning saturated: ok
read metric saturated: ok
read descriptor count stable: ok
destruction restored descriptor count: ok
debug injection rejected a closed handle
close restored descriptor count: ok
