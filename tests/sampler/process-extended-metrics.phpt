--TEST--
Sampler reads extended current-process metrics supported by each platform
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\UnsupportedMetricException;

$platform = PHP_OS_FAMILY;
$darwinCyclesAvailable = false;
if ($platform === 'Darwin') {
    try {
        $cycleProbe = Sampler::open([Metric::CpuCycles]);
        $cycleProbe->close();
        $darwinCyclesAvailable = true;
    } catch (UnsupportedMetricException) {
    }
}
$metrics = match ($platform) {
    'Linux' => [Metric::ContextSwitches],
    'Windows' => [Metric::CpuCycles],
    'Darwin' => $darwinCyclesAvailable
        ? [Metric::ContextSwitches, Metric::CpuCycles]
        : [Metric::ContextSwitches],
    default => throw new RuntimeException('Unsupported test platform'),
};

$readNativeContextSwitches = match ($platform) {
    'Linux', 'Darwin' => static function (): int {
        $usage = getrusage();
        return $usage['ru_nvcsw'] + $usage['ru_nivcsw'];
    },
    'Windows' => null,
};
$readNativeCycles = match ($platform) {
    'Linux' => null,
    'Windows' => static fn(): int => Perfidious\Windows\query_current_process_cycle_time(),
    'Darwin' => $darwinCyclesAvailable
        ? static fn(): int => Perfidious\Darwin\get_current_process_resource_usage()->cycleCount
        : null,
};
$withinNativeBounds = static fn(int $value, int $lower, int $upper): bool =>
    $value >= max(0, $lower) && $value <= $upper;

if ($readNativeContextSwitches !== null) {
    $contextOriginLowerBound = $readNativeContextSwitches();
}
if ($readNativeCycles !== null) {
    $cycleOriginLowerBound = $readNativeCycles();
}

$sampler = Sampler::open($metrics);
if ($readNativeContextSwitches !== null) {
    $contextOriginUpperBound = $readNativeContextSwitches();
    $contextBeforeLowerBound = $readNativeContextSwitches();
}
if ($readNativeCycles !== null) {
    $cycleOriginUpperBound = $readNativeCycles();
    $cycleBeforeLowerBound = $readNativeCycles();
}
$before = $sampler->read();
if ($readNativeContextSwitches !== null) {
    $contextBeforeUpperBound = $readNativeContextSwitches();
}
if ($readNativeCycles !== null) {
    $cycleBeforeUpperBound = $readNativeCycles();
}
$accumulator = 0;

for ($attempt = 0; $attempt < 4; $attempt++) {
    $deadline = hrtime(true) + 50_000_000;
    while (hrtime(true) < $deadline) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }
    usleep(10_000);

    if ($readNativeContextSwitches !== null) {
        $contextAfterLowerBound = $readNativeContextSwitches();
    }
    if ($readNativeCycles !== null) {
        $cycleAfterLowerBound = $readNativeCycles();
    }
    $after = $sampler->read();
    if ($readNativeContextSwitches !== null) {
        $contextAfterUpperBound = $readNativeContextSwitches();
    }
    if ($readNativeCycles !== null) {
        $cycleAfterUpperBound = $readNativeCycles();
    }

    $contextSwitchesMatchNative = $readNativeContextSwitches === null ||
        (
            $withinNativeBounds(
                $after->value(Metric::ContextSwitches),
                $contextAfterLowerBound - $contextOriginUpperBound,
                $contextAfterUpperBound - $contextOriginLowerBound,
            ) &&
            $withinNativeBounds(
                $after->value(Metric::ContextSwitches) - $before->value(Metric::ContextSwitches),
                $contextAfterLowerBound - $contextBeforeUpperBound,
                $contextAfterUpperBound - $contextBeforeLowerBound,
            )
        );
    $cyclesMatchNative = $readNativeCycles === null ||
        (
            $withinNativeBounds(
                $after->value(Metric::CpuCycles),
                $cycleAfterLowerBound - $cycleOriginUpperBound,
                $cycleAfterUpperBound - $cycleOriginLowerBound,
            ) &&
            $withinNativeBounds(
                $after->value(Metric::CpuCycles) - $before->value(Metric::CpuCycles),
                $cycleAfterLowerBound - $cycleBeforeUpperBound,
                $cycleAfterUpperBound - $cycleBeforeLowerBound,
            )
        );
    $nativeCyclesAdvancedOrUnavailable = $readNativeCycles === null ||
        $cycleAfterUpperBound > $cycleBeforeLowerBound;

    if ($contextSwitchesMatchNative && $cyclesMatchNative && $nativeCyclesAdvancedOrUnavailable) {
        break;
    }
}

$delta = $after->since($before);
$allValuesAreInts = true;
foreach ($metrics as $metric) {
    $allValuesAreInts = $allValuesAreInts &&
        is_int($before->value($metric)) &&
        is_int($after->value($metric)) &&
        is_int($delta->value($metric));
}

var_dump($sampler->metrics() === $metrics);
var_dump($allValuesAreInts);
var_dump($contextSwitchesMatchNative);
var_dump($cyclesMatchNative);
var_dump($nativeCyclesAdvancedOrUnavailable);
var_dump(
    !in_array(Metric::ContextSwitches, $metrics, true) ||
    $delta->value(Metric::ContextSwitches) ===
        $after->value(Metric::ContextSwitches) - $before->value(Metric::ContextSwitches)
);
var_dump(
    !in_array(Metric::CpuCycles, $metrics, true) ||
    $delta->value(Metric::CpuCycles) ===
        $after->value(Metric::CpuCycles) - $before->value(Metric::CpuCycles)
);
var_dump(is_int($accumulator));

$sampler->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
