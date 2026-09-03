<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;

$metrics = [Metric::CpuTime];
$nativeBefore = native_thread_cpu_time_ns();
$sampler = Sampler::open($metrics, Scope::CurrentThread);
$before = $sampler->read();
$nativeAfterBeforeSample = native_thread_cpu_time_ns();
$beforeCpuTimeNs = $before->value(Metric::CpuTime);
$firstSampleUpperBound = $nativeAfterBeforeSample - $nativeBefore;
$accumulator = 0;

for ($attempt = 0; $attempt < 3; $attempt++) {
    for ($iteration = 0; $iteration < 10_000_000; $iteration++) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    $nativeBeforeAfterSample = native_thread_cpu_time_ns();
    $after = $sampler->read();
    $nativeAfter = native_thread_cpu_time_ns();
    $samplerDelta = $after->value(Metric::CpuTime) - $before->value(Metric::CpuTime);
    $nativeLowerBound = $nativeBeforeAfterSample - $nativeAfterBeforeSample;
    $nativeUpperBound = $nativeAfter - $nativeBefore;

    if (
        $nativeLowerBound > 0 &&
        $samplerDelta >= $nativeLowerBound &&
        $samplerDelta <= $nativeUpperBound
    ) {
        break;
    }
}

$delta = $after->since($before);

var_dump($sampler->metrics() === $metrics);
var_dump($beforeCpuTimeNs >= 0);
var_dump($beforeCpuTimeNs <= $firstSampleUpperBound);
var_dump($after->value(Metric::CpuTime) > $before->value(Metric::CpuTime));
var_dump($delta->value(Metric::CpuTime) === $samplerDelta);
var_dump($nativeLowerBound > 0);
var_dump($samplerDelta >= $nativeLowerBound);
var_dump($samplerDelta <= $nativeUpperBound);
var_dump($delta->elapsedTimeNs > 0, is_int($accumulator));

$sampler->close();
