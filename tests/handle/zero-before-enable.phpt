--TEST--
Fresh Perfidious handles start at zero and remain stopped until enabled
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
function burnCpu(): void
{
    $deadline = hrtime(true) + 5_000_000;
    while (hrtime(true) < $deadline) {
        hash('sha256', 'fresh handle');
    }
}

$groups = [
    'empty' => [],
    'single' => ['perf::PERF_COUNT_SW_TASK_CLOCK:u'],
    'multiple' => [
        'perf::PERF_COUNT_SW_TASK_CLOCK:u',
        'perf::PERF_COUNT_SW_CPU_CLOCK:u',
        'perf::PERF_COUNT_SW_PAGE_FAULTS:u',
    ],
];

foreach ($groups as $label => $events) {
    echo "$label\n";
    for ($attempt = 0; $attempt < 2; $attempt++) {
        $handle = Perfidious\open($events);
        $before = $handle->read();
        burnCpu();
        $after = $handle->read();
        $zero = array_fill_keys($events, 0);
        var_dump($before->values === $zero && $after->values === $zero && $handle->readArray() === $zero);
        var_dump($before->timeEnabled === 0 && $after->timeEnabled === 0);
        var_dump($before->timeRunning === 0 && $after->timeRunning === 0);

        $handle->enable();
        burnCpu();
        $handle->disable();
        $handle->close();
    }
}
--EXPECT--
empty
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
single
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
multiple
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
