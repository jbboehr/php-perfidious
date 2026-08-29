--TEST--
Sampler value objects expose only their accessor API and cannot be copied or serialized
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sample;
use Perfidious\SampleDelta;
use Perfidious\Sampler;

$sampler = Sampler::open([Metric::CpuTime]);
$before = $sampler->read();
usleep(1_000);
$delta = $sampler->read()->since($before);

foreach ([$sampler, $before, $delta] as $object) {
    $class = new ReflectionClass($object);
    var_dump($class->isFinal(), !$class->isCloneable());

    try {
        clone $object;
    } catch (Error) {
        echo "cloning blocked\n";
    }

    try {
        serialize($object);
    } catch (Throwable) {
        echo "serialization blocked\n";
    }

    try {
        $object->dynamic = true;
    } catch (Error) {
        echo "dynamic property blocked\n";
    }
}

foreach ([Sampler::class, Sample::class, SampleDelta::class] as $class) {
    var_dump((new ReflectionClass($class))->getConstructor()->isPrivate());
}
var_dump((new ReflectionClass(Sample::class))->getProperties() === []);

$deltaProperties = (new ReflectionClass(SampleDelta::class))->getProperties();
var_dump(
    count($deltaProperties) === 1,
    $deltaProperties[0]->getName() === 'elapsedTimeNs',
    $deltaProperties[0]->isPublic(),
    $deltaProperties[0]->isReadOnly(),
);

try {
    $delta->elapsedTimeNs = 0;
} catch (Error) {
    echo "elapsed time readonly\n";
}

$sampler->close();
--EXPECT--
bool(true)
bool(true)
cloning blocked
serialization blocked
dynamic property blocked
bool(true)
bool(true)
cloning blocked
serialization blocked
dynamic property blocked
bool(true)
bool(true)
cloning blocked
serialization blocked
dynamic property blocked
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
elapsed time readonly
