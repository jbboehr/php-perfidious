--TEST--
Sampler rejects invalid, unavailable, and mismatched metric access
--EXTENSIONS--
perfidious
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
            $cycleProbe = Sampler::open([Metric::CpuCycles]);
            $cycleProbe->close();
            $darwinCyclesAvailable = true;
        } catch (UnsupportedMetricException) {
        }
    }
    $supportedExtendedMetrics = match (PHP_OS_FAMILY) {
        'Linux' => [Metric::ContextSwitches],
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
            $unexpectedSampler = Sampler::open([$unsupportedMetric]);
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

    Sampler::open($staticallyUnsupportedProcessMetrics);
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
        [Metric::CpuTime, Metric::PageFaults],
        $supportedExtendedMetrics,
        $staticallyUnsupportedProcessMetrics,
    ));
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
        Sampler::open([Metric::CpuCycles, Metric::Instructions]);
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

$afterRejectedRequest = Sampler::open([Metric::PageFaults, Metric::CpuTime]);
var_dump($afterRejectedRequest->metrics() === [Metric::PageFaults, Metric::CpuTime]);
$afterRejectedRequest->close();

$threadMetricSupportMatchesPlatform = true;
foreach (Metric::cases() as $metric) {
    $isSupported = (
        PHP_OS_FAMILY === 'Windows' &&
        in_array($metric, [Metric::CpuTime, Metric::ContextSwitches, Metric::CpuCycles], true)
    ) || (
        PHP_OS_FAMILY === 'Darwin' &&
        $metric === Metric::CpuTime
    );

    try {
        $threadSampler = Sampler::open([$metric], Scope::CurrentThread);
        $threadSampler->close();
        $threadMetricSupportMatchesPlatform = $threadMetricSupportMatchesPlatform && $isSupported;
    } catch (UnsupportedMetricException $exception) {
        $threadMetricSupportMatchesPlatform = $threadMetricSupportMatchesPlatform &&
            !$isSupported &&
            str_contains($exception->getMessage(), $metric->value) &&
            str_contains($exception->getMessage(), 'current-thread');
    }
}
var_dump($threadMetricSupportMatchesPlatform);

$mixedThreadRequestsRejectedWithoutProfileLeak = true;
if (PHP_OS_FAMILY === 'Windows') {
    foreach ([Metric::PageFaults, Metric::Instructions] as $unsupportedThreadMetric) {
        try {
            $unexpectedThreadSampler = Sampler::open(
                [Metric::ContextSwitches, $unsupportedThreadMetric, Metric::CpuCycles],
                Scope::CurrentThread,
            );
            $unexpectedThreadSampler->close();
            $mixedThreadRequestsRejectedWithoutProfileLeak = false;
        } catch (UnsupportedMetricException) {
        }

        $profileAfterRejectedRequest = Perfidious\Windows\enable_current_thread_profiling();
        $profileAfterRejectedRequest->close();
    }
}
var_dump($mixedThreadRequestsRejectedWithoutProfileLeak);

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
bool(true)
not collected
different sampler
reversed samples
bool(true)
closed
