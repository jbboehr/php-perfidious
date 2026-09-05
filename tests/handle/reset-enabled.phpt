--TEST--
Perfidious\Handle::reset() preserves whether counting is enabled
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$events = [
    'perf::PERF_COUNT_SW_TASK_CLOCK:u',
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
];

function burnCpu(): void
{
    $deadline = hrtime(true) + 5_000_000;
    while (hrtime(true) < $deadline) {
        hash('sha256', 'reset state');
    }
}

$handle = Perfidious\open($events)->enable();
$handle->reset();
burnCpu();
$handle->disable();
var_dump(count(array_filter($handle->readArray(), static fn(int $value): bool => $value > 0)) === count($events));

$before = $handle->read();
$handle->reset();
burnCpu();
$after = $handle->read();
var_dump($after->values === array_fill_keys($events, 0));
var_dump($after->timeEnabled === $before->timeEnabled);
var_dump($after->timeRunning === $before->timeRunning);
$handle->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
