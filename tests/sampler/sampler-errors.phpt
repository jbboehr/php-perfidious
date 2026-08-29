--TEST--
Sampler rejects invalid, unavailable, and mismatched metric access
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\IOException;
use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;
use Perfidious\UnsupportedMetricException;

foreach (
    [
        [],
        [Metric::CpuTime, Metric::CpuTime],
    ] as $metrics
) {
    try {
        Sampler::open($metrics);
    } catch (ValueError) {
        echo "invalid metric set\n";
    }
}

try {
    Sampler::open([Metric::CpuTime, 'page-faults']);
} catch (TypeError) {
    echo "invalid metric type\n";
}

try {
    Sampler::open([Metric::ContextSwitches, Metric::CpuCycles, Metric::Instructions]);
} catch (UnsupportedMetricException $exception) {
    var_dump(
        $exception instanceof Perfidious\ExceptionInterface,
        str_contains($exception->getMessage(), 'context-switches'),
        str_contains($exception->getMessage(), 'cpu-cycles'),
        str_contains($exception->getMessage(), 'instructions'),
        str_contains($exception->getMessage(), 'current-process')
    );
}

try {
    Sampler::open([
        Metric::CpuTime,
        Metric::CpuCycles,
        Metric::PageFaults,
        Metric::Instructions,
    ]);
} catch (UnsupportedMetricException $exception) {
    var_dump(
        str_contains($exception->getMessage(), 'cpu-cycles'),
        str_contains($exception->getMessage(), 'instructions')
    );
}

$afterRejectedRequest = Sampler::open([Metric::PageFaults, Metric::CpuTime]);
var_dump($afterRejectedRequest->metrics() === [Metric::PageFaults, Metric::CpuTime]);
$afterRejectedRequest->close();

try {
    Sampler::open([Metric::CpuTime], Scope::CurrentThread);
} catch (UnsupportedMetricException $exception) {
    var_dump(str_contains($exception->getMessage(), 'current-thread'));
}

$firstSampler = Sampler::open([Metric::CpuTime]);
$secondSampler = Sampler::open([Metric::CpuTime]);
$first = $firstSampler->read();
$second = $secondSampler->read();

try {
    $first->value(Metric::PageFaults);
} catch (ValueError) {
    echo "not collected\n";
}

try {
    $first->since($second);
} catch (ValueError) {
    echo "different sampler\n";
}

usleep(1_000);
$later = $firstSampler->read();
try {
    $first->since($later);
} catch (ValueError) {
    echo "reversed samples\n";
}

$firstSampler->close();
$firstSampler->close();
var_dump($firstSampler->metrics() === [Metric::CpuTime]);

try {
    $firstSampler->read();
} catch (IOException) {
    echo "closed\n";
}

$secondSampler->close();
--EXPECT--
invalid metric set
invalid metric set
invalid metric type
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
not collected
different sampler
reversed samples
bool(true)
closed
