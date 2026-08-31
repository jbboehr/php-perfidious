--TEST--
Sampler metrics expose their measurement units
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\MetricUnit;

if (!enum_exists(MetricUnit::class)) {
    echo "MetricUnit enum missing\n";
    return;
}

$reflection = new ReflectionEnum(MetricUnit::class);
var_dump(!$reflection->isBacked());
var_dump(MetricUnit::cases() === [MetricUnit::Nanoseconds, MetricUnit::Count]);

$unitMethods = array_map(
    static fn(ReflectionMethod $method): string => $method->getName(),
    $reflection->getMethods(),
);
sort($unitMethods);
var_dump($unitMethods === ['cases']);

$metricMethods = array_map(
    static fn(ReflectionMethod $method): string => $method->getName(),
    (new ReflectionEnum(Metric::class))->getMethods(),
);
sort($metricMethods);
var_dump($metricMethods === ['cases', 'from', 'tryFrom', 'unit']);

$expected = [
    [Metric::CpuTime, MetricUnit::Nanoseconds],
    [Metric::PageFaults, MetricUnit::Count],
    [Metric::ContextSwitches, MetricUnit::Count],
    [Metric::CpuCycles, MetricUnit::Count],
    [Metric::Instructions, MetricUnit::Count],
];

foreach ($expected as [$metric, $unit]) {
    printf("%s=%s\n", $metric->value, $metric->unit()->name);
    var_dump($metric->unit() === $unit);
}

$method = new ReflectionMethod(Metric::class, 'unit');
var_dump(
    $method->isPublic(),
    !$method->isStatic(),
    $method->getNumberOfRequiredParameters() === 0,
    $method->getNumberOfParameters() === 0,
    (string) $method->getReturnType() === MetricUnit::class,
);

try {
    Metric::CpuTime->unit(null);
    echo "unexpected argument accepted\n";
} catch (ArgumentCountError) {
    echo "unexpected argument rejected\n";
}
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
cpu-time=Nanoseconds
bool(true)
page-faults=Count
bool(true)
context-switches=Count
bool(true)
cpu-cycles=Count
bool(true)
instructions=Count
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
unexpected argument rejected
