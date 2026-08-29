--TEST--
Sampler reads requested current-process CPU time and page faults
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sample;
use Perfidious\SampleDelta;
use Perfidious\Sampler;

$metrics = [Metric::CpuTime, Metric::PageFaults];
$sampler = Sampler::open($metrics);
$before = $sampler->read();
$pages = [];
$accumulator = 0;

for ($attempt = 0; $attempt < 4; $attempt++) {
    $pages[] = str_repeat('x', 8 * 1024 * 1024);

    $deadline = hrtime(true) + 30_000_000;
    while (hrtime(true) < $deadline) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    $after = $sampler->read();
    if (
        $after->value(Metric::CpuTime) > $before->value(Metric::CpuTime) &&
        $after->value(Metric::PageFaults) > $before->value(Metric::PageFaults)
    ) {
        break;
    }
}

$delta = $after->since($before);

var_dump($sampler->metrics() === $metrics);
var_dump($before instanceof Sample, $after instanceof Sample, $delta instanceof SampleDelta);
var_dump(get_object_vars($after));
var_dump($after->value(Metric::CpuTime) > $before->value(Metric::CpuTime));
var_dump($after->value(Metric::PageFaults) > $before->value(Metric::PageFaults));
var_dump(
    $delta->value(Metric::CpuTime) ===
        $after->value(Metric::CpuTime) - $before->value(Metric::CpuTime)
);
var_dump(
    $delta->value(Metric::PageFaults) ===
        $after->value(Metric::PageFaults) - $before->value(Metric::PageFaults)
);
var_dump($delta->elapsedTimeNs > 0);
var_dump(is_int($accumulator));

$sampler->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
array(0) {
}
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
