#!/usr/bin/env php
<?php
/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

use function Perfidious\open;

// If specifying pid, you may need to grant cap_perfmon
// sudo capsh --caps="cap_perfmon,cap_setgid,cap_setuid,cap_setpcap+eip" --user=`whoami` --addamb='cap_perfmon' -- \
//    -c 'php -dextension=modules/perfidious.so examples/watch.php --interval 2 --pid 3319'

$rest_index = null;
$opts = getopt('', [
    'pid:',
    'cpu:',
    'interval:'
], $rest_index);
$pos_args = array_slice($argv, $rest_index);

if (count($pos_args) <= 0) {
    $pos_args = [
        'perf::PERF_COUNT_HW_CPU_CYCLES:u',
        'perf::PERF_COUNT_HW_INSTRUCTIONS:u',
    ];
}

$pid = $opts['pid'] ?? 0;
$pid = is_numeric($pid) ? (int) $pid : 0;
$cpu = $opts['cpu'] ?? -1;
$cpu = is_numeric($cpu) ? (int) $cpu : -1;
$interval = $opts['interval'] ?? 2;
$interval = is_numeric($interval) ? (int) $interval : 2;
$interval *= 1000000;

$handle = open($pos_args, $pid, $cpu);
$handle->enable();


while (true) { // @phpstan-ignore-line
    $stats = $handle->read();
    $percent_running = $stats->timeEnabled > 0 ? 100 * $stats->timeRunning / $stats->timeEnabled : 0;

    printf("cpu=%d pid=%d\n", $cpu, $pid);
    printf("time_enabled=%d time_running=%d percent_running=%d%%\n", $stats->timeEnabled, $stats->timeRunning, $percent_running);
    foreach ($stats->values as $k => $v) {
        printf("%s=%d\n", $k, $v);
    }

    usleep($interval);
}
