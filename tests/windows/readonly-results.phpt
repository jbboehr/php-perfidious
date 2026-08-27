--TEST--
Perfidious Windows result objects are output-only and immutable
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

$profile = Perfidious\Windows\enable_current_thread_profiling();
$results = [
    Perfidious\Windows\get_current_process_times(),
    Perfidious\Windows\get_current_process_memory_info(),
    $profile->read(),
];

foreach ($results as $result) {
    $reflection = new ReflectionClass($result);
    $properties = $reflection->getProperties(ReflectionProperty::IS_PUBLIC);

    var_dump($reflection->isFinal());
    var_dump($reflection->getConstructor()?->isPrivate());
    var_dump(array_reduce(
        $properties,
        static fn(bool $readonly, ReflectionProperty $property): bool => $readonly && $property->isReadOnly(),
        true
    ));

    $property = $properties[0]->getName();
    try {
        $result->$property = 0;
    } catch (Error) {
        echo "readonly\n";
    }

    try {
        $result->extra = 0;
    } catch (Error) {
        echo "no dynamic properties\n";
    }
}

foreach ([
    Perfidious\Windows\ProcessTimes::class,
    Perfidious\Windows\ProcessMemoryInfo::class,
    Perfidious\Windows\ThreadProfileSnapshot::class,
    Perfidious\Windows\HardwareCounterSnapshot::class,
] as $class) {
    var_dump((new ReflectionClass($class))->getConstructor()?->isPrivate());
}

$profile->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
readonly
no dynamic properties
bool(true)
bool(true)
bool(true)
readonly
no dynamic properties
bool(true)
bool(true)
bool(true)
readonly
no dynamic properties
bool(true)
bool(true)
bool(true)
bool(true)
