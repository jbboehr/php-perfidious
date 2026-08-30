--TEST--
UnsupportedMetricException exposes structured recovery metadata
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;
use Perfidious\ExceptionInterface;
use Perfidious\UnsupportedMetricException;

$requestedMetrics = [Metric::Instructions, Metric::PageFaults];

try {
    Sampler::open($requestedMetrics, Scope::CurrentThread);
} catch (UnsupportedMetricException $exception) {
    var_dump($exception->scope === Scope::CurrentThread);
    var_dump($exception->unsupportedMetrics === $requestedMetrics);
    $scopeProperty = new ReflectionProperty($exception, 'scope');
    $unsupportedMetricsProperty = new ReflectionProperty($exception, 'unsupportedMetrics');
    var_dump($scopeProperty->isInitialized($exception), $unsupportedMetricsProperty->isInitialized($exception));
    var_dump($scopeProperty->isPublic(), $scopeProperty->isReadOnly(), (string) $scopeProperty->getType() === Scope::class);
    var_dump(
        $unsupportedMetricsProperty->isPublic(),
        $unsupportedMetricsProperty->isReadOnly(),
        (string) $unsupportedMetricsProperty->getType() === 'array',
        array_is_list($exception->unsupportedMetrics),
        $exception->unsupportedMetrics !== [],
    );
    var_dump((new ReflectionMethod($exception, '__construct'))->isPrivate());
    var_dump(
        $exception::class === UnsupportedMetricException::class,
        $exception instanceof RuntimeException,
        $exception instanceof ExceptionInterface,
        (new ReflectionClass($exception))->isFinal(),
        $exception->getMessage() ===
            'Metrics [instructions, page-faults] are not supported for scope current-thread',
    );

    try {
        $exception->scope = Scope::CurrentProcess;
    } catch (Error $error) {
        echo "scope is readonly\n";
    }

    try {
        $exception->unsupportedMetrics[] = Metric::CpuTime;
    } catch (Error $error) {
        echo "unsupported metrics are readonly\n";
    }

    try {
        serialize($exception);
        echo "serialization unexpectedly allowed\n";
    } catch (Throwable) {
        echo "serialization blocked\n";
    }
}

try {
    new UnsupportedMetricException();
} catch (Error) {
    echo "constructor is private\n";
}

[$mixedRequest, $expectedUnsupportedMetrics] = match (PHP_OS_FAMILY) {
    'Linux' => [
        [Metric::Instructions, Metric::CpuTime, Metric::CpuCycles, Metric::PageFaults],
        [Metric::Instructions, Metric::CpuCycles],
    ],
    'Windows' => [
        [Metric::Instructions, Metric::CpuTime, Metric::ContextSwitches, Metric::PageFaults],
        [Metric::Instructions, Metric::ContextSwitches],
    ],
    'Darwin' => [
        [Metric::CpuTime, Metric::Instructions, Metric::PageFaults],
        [Metric::Instructions],
    ],
    default => throw new RuntimeException('Unsupported test platform'),
};

try {
    $unexpectedSampler = Sampler::open($mixedRequest);
    $unexpectedSampler->close();
    echo "mixed request unexpectedly supported\n";
} catch (UnsupportedMetricException $exception) {
    $expectedMessage = sprintf(
        'Metrics [%s] are not supported for scope current-process',
        implode(', ', array_map(static fn(Metric $metric): string => $metric->value, $expectedUnsupportedMetrics)),
    );

    var_dump(
        $exception->scope === Scope::CurrentProcess,
        $exception->unsupportedMetrics === $expectedUnsupportedMetrics,
        array_keys($exception->unsupportedMetrics) === array_keys($expectedUnsupportedMetrics),
        $exception->getMessage() === $expectedMessage,
    );
}

$class = UnsupportedMetricException::class;
$serializedWithoutMetadata = sprintf('O:%d:"%s":0:{}', strlen($class), $class);
try {
    unserialize($serializedWithoutMetadata);
    echo "unserialization unexpectedly allowed\n";
} catch (Throwable) {
    echo "unserialization blocked\n";
}

set_exception_handler(static function (Throwable $exception): void {
    var_dump(
        $exception instanceof UnsupportedMetricException,
        $exception->scope === Scope::CurrentProcess,
        $exception->unsupportedMetrics === [Metric::Instructions],
    );
});

Sampler::open([Metric::CpuTime, Metric::Instructions]);
echo "exception hook was not called\n";
?>
--EXPECT--
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
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
scope is readonly
unsupported metrics are readonly
serialization blocked
constructor is private
bool(true)
bool(true)
bool(true)
bool(true)
unserialization blocked
bool(true)
bool(true)
bool(true)
