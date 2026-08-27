--TEST--
Perfidious Windows result objects can be serialized as values
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
    $copy = unserialize(serialize($result));
    var_dump($copy == $result);
}

$profile->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
