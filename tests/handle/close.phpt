--TEST--
Perfidious\Handle::close() releases owned descriptors and rejects further use
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--FILE--
<?php

use Perfidious\ClosedException;

function openDescriptorCount(): int
{
    return count(scandir('/proc/self/fd')) - 2;
}

$baseline = openDescriptorCount();
$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
    'perf::PERF_COUNT_SW_PAGE_FAULTS:u',
]);
printf("handle descriptors: %d\n", openDescriptorCount() - $baseline);

$stream = $handle->rawStream();
printf("including raw stream: %d\n", openDescriptorCount() - $baseline);

var_dump($handle->close());
printf("after handle close: %d\n", openDescriptorCount() - $baseline);
printf("raw stream survives: %d bytes\n", strlen(fread($stream, 32)));

var_dump($handle->close());

foreach (
    [
        'enable' => static fn() => $handle->enable(),
        'disable' => static fn() => $handle->disable(),
        'reset' => static fn() => $handle->reset(),
        'read' => static fn() => $handle->read(),
        'readArray' => static fn() => $handle->readArray(),
        'rawStream' => static fn() => $handle->rawStream(),
    ] as $method => $call
) {
    try {
        $call();
        echo "$method unexpectedly succeeded\n";
    } catch (ClosedException) {
        echo "$method rejected closed handle\n";
    }
}

fclose($stream);
printf("after stream close: %d\n", openDescriptorCount() - $baseline);
?>
--EXPECT--
handle descriptors: 3
including raw stream: 4
NULL
after handle close: 1
raw stream survives: 32 bytes
NULL
enable rejected closed handle
disable rejected closed handle
reset rejected closed handle
read rejected closed handle
readArray rejected closed handle
rawStream rejected closed handle
after stream close: 0
