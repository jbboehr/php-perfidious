--TEST--
Perfidious exceptions distinguish lifecycle misuse, thread misuse, resource conflicts, and native I/O
--EXTENSIONS--
perfidious
--FILE--
<?php

$contracts = [
    'Perfidious\\ClosedException' => LogicException::class,
    'Perfidious\\WrongThreadException' => LogicException::class,
    'Perfidious\\ResourceBusyException' => RuntimeException::class,
];

foreach ($contracts as $class => $parent) {
    $reflection = class_exists($class) ? new ReflectionClass($class) : null;
    $valid = $reflection !== null &&
        $reflection->isFinal() &&
        $reflection->isSubclassOf($parent) &&
        $reflection->implementsInterface(Perfidious\ExceptionInterface::class) &&
        !$reflection->isSubclassOf(Perfidious\IOException::class);

    printf("%s: %s\n", substr($class, strlen('Perfidious\\')), $valid ? 'ok' : 'missing or invalid');
}
--EXPECT--
ClosedException: ok
WrongThreadException: ok
ResourceBusyException: ok
