--TEST--
Perfidious Windows direct counters increase after work
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

$processCyclesBefore = Perfidious\Windows\query_current_process_cycle_time();
$threadCyclesBefore = Perfidious\Windows\query_current_thread_cycle_time();
$timesBefore = Perfidious\Windows\get_current_process_times();
$memoryBefore = Perfidious\Windows\get_current_process_memory_info();

$pages = [];
$allocatedBytes = 0;
$accumulator = 0;

for ($attempt = 0; $attempt < 3; $attempt++) {
    $pages[] = str_repeat('x', 32 * 1024 * 1024);
    $allocatedBytes += strlen($pages[array_key_last($pages)]);

    $deadline = hrtime(true) + 100_000_000;
    while (hrtime(true) < $deadline) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    $processCyclesAfter = Perfidious\Windows\query_current_process_cycle_time();
    $threadCyclesAfter = Perfidious\Windows\query_current_thread_cycle_time();
    $timesAfter = Perfidious\Windows\get_current_process_times();
    $memoryAfter = Perfidious\Windows\get_current_process_memory_info();

    if (
        $processCyclesAfter > $processCyclesBefore &&
        $threadCyclesAfter > $threadCyclesBefore &&
        $timesAfter->kernelTime100ns + $timesAfter->userTime100ns >
            $timesBefore->kernelTime100ns + $timesBefore->userTime100ns &&
        $memoryAfter->pageFaultCount > $memoryBefore->pageFaultCount
    ) {
        break;
    }
}

var_dump($processCyclesAfter > $processCyclesBefore);
var_dump($threadCyclesAfter > $threadCyclesBefore);
var_dump(
    $timesAfter->kernelTime100ns + $timesAfter->userTime100ns >
    $timesBefore->kernelTime100ns + $timesBefore->userTime100ns
);
var_dump($memoryAfter->pageFaultCount > $memoryBefore->pageFaultCount);
var_dump($allocatedBytes >= 32 * 1024 * 1024, is_int($accumulator));
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
