--TEST--
Perfidious Windows result objects can be serialized as values
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

$results = [
    Perfidious\Windows\get_current_process_times(),
    Perfidious\Windows\get_current_thread_times(),
    Perfidious\Windows\get_current_process_memory_info(),
];

foreach ($results as $result) {
    $copy = unserialize(serialize($result));
    var_dump($copy == $result);
}

--EXPECT--
bool(true)
bool(true)
bool(true)
