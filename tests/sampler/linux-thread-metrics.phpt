--TEST--
Linux sampler defaults to the current thread and reads every available perf metric
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/skipif-perf-permissions.inc';
?>
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;
use Perfidious\UnsupportedMetricException;

$metrics = [Metric::CpuTime, Metric::PageFaults, Metric::ContextSwitches];
foreach ([Metric::CpuCycles, Metric::Instructions] as $metric) {
    try {
        $probe = Sampler::open([$metric]);
        $probe->close();
        $metrics[] = $metric;
    } catch (UnsupportedMetricException $error) {
        if ($error->scope !== Scope::CurrentThread || $error->unsupportedMetrics !== [$metric]) {
            throw new RuntimeException('Unavailable hardware metadata does not match the request');
        }
    }
}

$sampler = Sampler::open($metrics);
$before = $sampler->read();
$pages = str_repeat('x', 8 * 1024 * 1024);
$deadline = hrtime(true) + 100_000_000;
do {
    $digest = hash('sha256', $pages);
} while (hrtime(true) < $deadline);
usleep(10_000);
$after = $sampler->read();
$delta = $after->since($before);
foreach ($metrics as $metric) {
    if ($delta->value($metric) <= 0) {
        throw new RuntimeException($metric->value . ' did not advance');
    }
    if ($delta->value($metric) !== $after->value($metric) - $before->value($metric)) {
        throw new RuntimeException($metric->value . ' delta does not match its samples');
    }
}
var_dump($sampler->metrics() === $metrics, strlen($digest) === 64);
$sampler->close();
try {
    Sampler::open(Metric::cases(), Scope::CurrentProcess);
    throw new RuntimeException('Linux process scope was accepted');
} catch (UnsupportedMetricException $error) {
    var_dump($error->scope === Scope::CurrentProcess, $error->unsupportedMetrics === Metric::cases());
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
