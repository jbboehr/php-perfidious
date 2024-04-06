<?php

// ok, these aren't all examples per se
// this one tries to detect leaks

use function Perfidious\open;

$rest_index = null;
$opts = getopt('', [
    'count:',
], $rest_index);
$pos_args = array_slice($argv, $rest_index);

if (count($pos_args) <= 0) {
    $pos_args = [
        'perf::PERF_COUNT_HW_CPU_CYCLES:u',
        'perf::PERF_COUNT_HW_INSTRUCTIONS:u',
    ];
}

$start = time();
$last = $start;
$interval = 2;
$end = $start + (60 * 5);

$handle = open($pos_args);
$handle->enable();

$prev_memory = memory_get_usage();
$prev_memory_real = memory_get_usage(true);

do {
    $now = time();
    $stats = $handle->read();
    $arr = $handle->readArray();

    if ($now - $interval > $last) {
        $handle->disable();

        $last = $now;

        gc_collect_cycles();

        $memory = memory_get_usage();
        $memory_real = memory_get_usage(true);

        foreach ($stats->values as $k => $v) {
            printf("%s=%d\n", $k, $v);
        }

        printf("memory=%d last_memory=%d delta_memory=%d\n", $memory, $prev_memory, $memory - $prev_memory);
        printf("memory=%d last_memory=%d delta_memory=%d (real)\n", $memory, $prev_memory, $memory - $prev_memory);

        $prev_memory = $memory;
        $prev_memory_real = $memory_real;

        $handle->enable();
    }

    usleep(100_000);
} while ($now < $end);
