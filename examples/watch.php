#!/usr/bin/env php
<?php

use function Perfidious\open;

// If specifying pid, you may need to grant cap_perfmon
// sudo capsh --caps="cap_perfmon,cap_setgid,cap_setuid,cap_setpcap+eip" --user=`whoami` --addamb='cap_perfmon' -- -c 'php -dextension=modules/perfidious.so examples/watch.php --interval 2 --pid 3319'

$rest_index = null;
$opts = getopt('', [
    'pid:',
    'cpu:',
    'interval:'
], $rest_index);
$pos_args = array_slice($argv, $rest_index);

if (count($pos_args) <= 0) {
    $pos_args = [
        'perf::PERF_COUNT_HW_CPU_CYCLES',
        'perf::PERF_COUNT_HW_INSTRUCTIONS',
    ];
}

$pid = $opts['pid'] ?? 0;
$cpu = $opts['cpu'] ?? -1;
$interval = (float) ($opts['interval'] ?? 0) ?: 2;
$interval *= 1000000;

$handle = open($pos_args, $pid, $cpu);
$handle->enable();

while (true) {
    $stats = $handle->read();
    $percent_running = $stats->timeEnabled > 0 ? 100 * $stats->timeRunning / $stats->timeEnabled : 0;

    printf("cpu=%d pid=%d\n", $cpu, $pid);
    printf("time_enabled=%d time_running=%d percent_running=%d%%\n", $stats->timeEnabled, $stats->timeRunning, $percent_running);
    foreach ($stats->values as $k => $v) {
        printf("%s=%d\n", $k, $v);
    }

    usleep($interval);
}

foreach ($tmp as $k => $v) {
    echo $k;
}
