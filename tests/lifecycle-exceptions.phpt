--TEST--
Closed handles and samplers throw Perfidious\ClosedException
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--FILE--
<?php

$handle = Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u']);
$handle->close();

$sampler = Perfidious\Sampler::open([Perfidious\Metric::CpuTime]);
$sampler->close();

foreach (
    [
        'Handle' => static fn() => $handle->read(),
        'Sampler' => static fn() => $sampler->read(),
    ] as $object => $read
) {
    try {
        $read();
        echo "$object unexpectedly remained readable\n";
    } catch (Perfidious\ClosedException $exception) {
        printf(
            "%s: %s, native I/O: %s\n",
            $object,
            $exception::class,
            $exception instanceof Perfidious\IOException ? 'yes' : 'no',
        );
    }
}
--EXPECT--
Handle: Perfidious\ClosedException, native I/O: no
Sampler: Perfidious\ClosedException, native I/O: no
