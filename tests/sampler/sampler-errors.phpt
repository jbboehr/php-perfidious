--TEST--
Sampler rejects invalid, unavailable, and mismatched metric access
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/skipif-perf-permissions.inc';
?>
--FILE--
<?php

use Perfidious\ClosedException;
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
    $darwinCyclesAvailable = false;
    if (PHP_OS_FAMILY === 'Darwin') {
        try {
            $cycleProbe = Sampler::open([Metric::CpuCycles], Scope::CurrentProcess);
            $cycleProbe->close();
            $darwinCyclesAvailable = true;
        } catch (UnsupportedMetricException) {
        }
    }
    $supportedExtendedMetrics = match (PHP_OS_FAMILY) {
        'Linux' => [],
        'Windows' => [Metric::CpuCycles],
        'Darwin' => $darwinCyclesAvailable
            ? [Metric::ContextSwitches, Metric::CpuCycles]
            : [Metric::ContextSwitches],
        default => throw new RuntimeException('Unsupported test platform'),
    };
    $staticallyUnsupportedProcessMetrics = match (PHP_OS_FAMILY) {
        'Linux' => [Metric::CpuCycles, Metric::Instructions],
        'Windows' => [Metric::ContextSwitches, Metric::Instructions],
        'Darwin' => [Metric::Instructions],
        default => throw new RuntimeException('Unsupported test platform'),
    };
    $dynamicallyUnsupportedProcessMetrics = PHP_OS_FAMILY === 'Darwin' && !$darwinCyclesAvailable
        ? [Metric::CpuCycles]
        : [];
    $unsupportedProcessMetrics = array_merge(
        $staticallyUnsupportedProcessMetrics,
        $dynamicallyUnsupportedProcessMetrics,
    );

    $eachUnsupportedProcessMetricRejectedIndependently = true;
    foreach ($unsupportedProcessMetrics as $unsupportedMetric) {
        try {
            $unexpectedSampler = Sampler::open([$unsupportedMetric], Scope::CurrentProcess);
            $unexpectedSampler->close();
            $eachUnsupportedProcessMetricRejectedIndependently = false;
        } catch (UnsupportedMetricException $exception) {
            $message = $exception->getMessage();
            $eachUnsupportedProcessMetricRejectedIndependently =
                $eachUnsupportedProcessMetricRejectedIndependently &&
                str_contains($message, $unsupportedMetric->value) &&
                str_contains($message, 'current-process');

            foreach ($unsupportedProcessMetrics as $otherUnsupportedMetric) {
                if ($otherUnsupportedMetric !== $unsupportedMetric) {
                    $eachUnsupportedProcessMetricRejectedIndependently =
                        $eachUnsupportedProcessMetricRejectedIndependently &&
                        !str_contains($message, $otherUnsupportedMetric->value);
                }
            }
        }
    }
    var_dump($eachUnsupportedProcessMetricRejectedIndependently);

    Sampler::open($staticallyUnsupportedProcessMetrics, Scope::CurrentProcess);
} catch (UnsupportedMetricException $exception) {
    $message = $exception->getMessage();
    $listsEveryUnsupportedMetric = true;
    foreach ($staticallyUnsupportedProcessMetrics as $metric) {
        $listsEveryUnsupportedMetric = $listsEveryUnsupportedMetric && str_contains($message, $metric->value);
    }

    var_dump(
        $exception instanceof Perfidious\ExceptionInterface,
        $listsEveryUnsupportedMetric,
        str_contains($message, 'current-process')
    );
}

try {
    Sampler::open(array_merge(
        PHP_OS_FAMILY === 'Linux' ? [] : [Metric::CpuTime, Metric::PageFaults],
        $supportedExtendedMetrics,
        $staticallyUnsupportedProcessMetrics,
    ), Scope::CurrentProcess);
} catch (UnsupportedMetricException $exception) {
    $message = $exception->getMessage();
    $listsEveryUnsupportedMetric = true;
    foreach ($staticallyUnsupportedProcessMetrics as $metric) {
        $listsEveryUnsupportedMetric = $listsEveryUnsupportedMetric && str_contains($message, $metric->value);
    }
    $omitsEverySupportedMetric = true;
    foreach ($supportedExtendedMetrics as $metric) {
        $omitsEverySupportedMetric = $omitsEverySupportedMetric && !str_contains($message, $metric->value);
    }

    var_dump($listsEveryUnsupportedMetric, $omitsEverySupportedMetric);
}

if (PHP_OS_FAMILY === 'Darwin') {
    try {
        Sampler::open([Metric::CpuCycles, Metric::Instructions], Scope::CurrentProcess);
        var_dump(false);
    } catch (UnsupportedMetricException $exception) {
        var_dump(
            str_contains($exception->getMessage(), Metric::Instructions->value) &&
            !str_contains($exception->getMessage(), Metric::CpuCycles->value)
        );
    }
} else {
    var_dump(true);
}

$afterRejectedRequest = Sampler::open([Metric::PageFaults, Metric::CpuTime],
    PHP_OS_FAMILY === 'Linux' ? Scope::CurrentThread : Scope::CurrentProcess);
var_dump($afterRejectedRequest->metrics() === [Metric::PageFaults, Metric::CpuTime]);
$afterRejectedRequest->close();

$threadMetricSupportMatchesPlatform = true;
$threadMetrics = PHP_OS_FAMILY === 'Windows'
    ? [Metric::CpuTime, Metric::PageFaults, Metric::Instructions]
    : Metric::cases();
foreach ($threadMetrics as $metric) {
    $isSupported = PHP_OS_FAMILY === 'Linux' || (
        PHP_OS_FAMILY === 'Windows' &&
        $metric === Metric::CpuTime
    ) || (
        PHP_OS_FAMILY === 'Darwin' &&
        $metric === Metric::CpuTime
    );

    try {
        $threadSampler = Sampler::open([$metric], Scope::CurrentThread);
        $threadSampler->close();
        $threadMetricSupportMatchesPlatform = $threadMetricSupportMatchesPlatform && $isSupported;
    } catch (UnsupportedMetricException $exception) {
        $isOptionalHardware = PHP_OS_FAMILY === 'Linux' &&
            in_array($metric, [Metric::CpuCycles, Metric::Instructions], true);
        $threadMetricSupportMatchesPlatform = $threadMetricSupportMatchesPlatform &&
            (!$isSupported || $isOptionalHardware) &&
            str_contains($exception->getMessage(), $metric->value) &&
            str_contains($exception->getMessage(), 'current-thread');
    }
}
var_dump($threadMetricSupportMatchesPlatform);

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
} catch (ClosedException) {
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
