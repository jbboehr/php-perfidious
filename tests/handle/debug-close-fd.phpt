--TEST--
Perfidious\Handle::debugCloseFd() exercises read and explicit ioctl failures
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--FILE--
<?php
function openDescriptorCount(): int
{
    return count(scandir('/proc/self/fd')) - 2;
}

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

foreach (["reset", "enable", "disable"] as $operation) {
    try {
        $handle->$operation();
    } catch (Perfidious\IOException $e) {
        echo "$operation: ", $e->getMessage(), "\n";
    }
}

try {
    $handle->close();
} catch (Perfidious\IOException $e) {
    echo $e->getMessage(), "\n";
}

$handle->close();
echo "closed after failure\n";

foreach (
    [
        'debugCorruptMetricIds' => static fn() => $handle->debugCorruptMetricIds(),
        'debugCloseFd' => static fn() => $handle->debugCloseFd(),
    ] as $method => $call
) {
    try {
        $call();
        echo "$method unexpectedly succeeded\n";
    } catch (Perfidious\ClosedException) {
        echo "$method rejected closed handle\n";
    }
}

$probes = [tmpfile(), tmpfile()];
unset($call, $e, $handle);
gc_collect_cycles();

foreach ($probes as $idx => $probe) {
    printf("probe %d after destruction: %d bytes\n", $idx, fwrite($probe, "still open"));
}

$baseline = openDescriptorCount();
$cleanupHandle = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$cleanupHandle->debugCloseFd(1);

try {
    $cleanupHandle->close();
} catch (Perfidious\IOException $e) {
    echo "mid-close failure: ", $e->getMessage(), "\n";
}

printf("descriptors after mid-close failure: %d\n", openDescriptorCount() - $baseline);
$cleanupHandle->close();
--EXPECTF--
failed to read: Bad file descriptor
failed to read: Bad file descriptor
reset: ioctl failed: Bad file descriptor
enable: ioctl failed: Bad file descriptor
disable: ioctl failed: Bad file descriptor
close failed: Bad file descriptor
closed after failure
debugCorruptMetricIds rejected closed handle
debugCloseFd rejected closed handle
probe 0 after destruction: 10 bytes
probe 1 after destruction: 10 bytes
mid-close failure: close failed: Bad file descriptor
descriptors after mid-close failure: 0
