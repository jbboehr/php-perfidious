#!/usr/bin/env php
<?php

// ok, these aren't all examples per se
// this one runs the sieve of eratosthenes and outputs the cycles/instructions
// array edition

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

$count = $opts['count'] ?? 2000000;
$count = is_numeric($opts['count']) ? (int) $opts['count'] : 2000000;

$handle = open($pos_args);
$handle->enable();

$primes = sieve($count);

$handle->disable();
$stats = $handle->read();

var_dump($primes);
var_dump($stats);

/**
 * @return list<int>
 */
function sieve(int $n): array
{
    $lut = array_fill(0, $n, null);
    for ($i = 2; $i < $n; $i++) {
        for ($j = $i + $i; $j < $n; $j += $i) {
            $lut[$j] = true;
        }
    }
    $rv = [];
    for ($i = 2; $i < $n; $i++) {
        if (null === $lut[$i]) {
            $rv[] = $i;
        }
    }
    return $rv;
}
