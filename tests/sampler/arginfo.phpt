--TEST--
Sampler reflection signatures match the public API
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sample;
use Perfidious\SampleDelta;
use Perfidious\Sampler;
use Perfidious\Scope;

$expected = [
    [Sampler::class, 'open', true, 1, Sampler::class, ['array', Scope::class]],
    [Sampler::class, 'metrics', false, 0, 'array', []],
    [Sampler::class, 'read', false, 0, Sample::class, []],
    [Sampler::class, 'close', false, 0, 'void', []],
    [Sample::class, 'value', false, 1, 'int', [Metric::class]],
    [Sample::class, 'since', false, 1, SampleDelta::class, [Sample::class]],
    [SampleDelta::class, 'value', false, 1, 'int', [Metric::class]],
];

foreach ($expected as [$class, $name, $static, $required, $returnType, $parameterTypes]) {
    $method = new ReflectionMethod($class, $name);
    $actualParameterTypes = array_map(
        static fn(ReflectionParameter $parameter): string => (string) $parameter->getType(),
        $method->getParameters(),
    );

    var_dump([
        $method->isPublic(),
        $method->isStatic(),
        $method->getNumberOfRequiredParameters(),
        (string) $method->getReturnType(),
        $actualParameterTypes,
    ] === [true, $static, $required, $returnType, $parameterTypes]);
}

$scope = (new ReflectionMethod(Sampler::class, 'open'))->getParameters()[1];
var_dump(
    $scope->isOptional(),
    !$scope->allowsNull(),
    $scope->isDefaultValueConstant(),
    $scope->getDefaultValueConstantName() === Scope::class . '::CurrentProcess',
    $scope->getDefaultValue() === Scope::CurrentProcess,
);
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
