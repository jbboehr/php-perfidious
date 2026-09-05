--TEST--
Darwin process CPU-time APIs agree with getrusage nanosecond deltas
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-darwin-only.inc';
if (!function_exists('getrusage')) {
    die('skip: getrusage is unavailable');
}
?>
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;

function oracleNs(): int
{
    $usage = getrusage();
    if ($usage === false) {
        throw new RuntimeException('getrusage failed');
    }
    return ($usage['ru_utime.tv_sec'] + $usage['ru_stime.tv_sec']) * 1_000_000_000
        + ($usage['ru_utime.tv_usec'] + $usage['ru_stime.tv_usec']) * 1_000;
}

$sampler = Sampler::open([Metric::CpuTime]);
$outerBefore = oracleNs();
$usageBefore = Perfidious\Darwin\get_current_process_resource_usage();
$sampleBefore = $sampler->read();
$innerBefore = oracleNs();
$deadline = hrtime(true) + 200_000_000;
do {
    hash('sha256', 'Darwin CPU-time units');
} while (hrtime(true) < $deadline);
$innerAfter = oracleNs();
$sampleAfter = $sampler->read();
$usageAfter = Perfidious\Darwin\get_current_process_resource_usage();
$outerAfter = oracleNs();
$sampler->close();

// Bracket both APIs with an independent seconds/microseconds source.
// Allow 2 ms for differences in accounting precision between the native APIs.
$lower = $innerAfter - $innerBefore - 2_000_000;
$upper = $outerAfter - $outerBefore + 2_000_000;
if ($lower <= 0) {
    throw new RuntimeException('getrusage CPU-time oracle did not advance enough');
}
$deltas = [
    $usageAfter->userTimeNs + $usageAfter->systemTimeNs - $usageBefore->userTimeNs - $usageBefore->systemTimeNs,
    $sampleAfter->since($sampleBefore)->value(Metric::CpuTime),
];
foreach ($deltas as $delta) {
    if ($delta < $lower || $delta > $upper) {
        throw new RuntimeException("CPU-time delta $delta ns is outside getrusage bounds [$lower, $upper]");
    }
}
echo "process APIs agree with getrusage\n";
?>
--EXPECT--
process APIs agree with getrusage
