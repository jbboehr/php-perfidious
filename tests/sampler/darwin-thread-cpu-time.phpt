--TEST--
Sampler reads current-thread CPU time on Darwin
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-darwin-only.inc'; ?>
--FILE--
<?php

function native_thread_cpu_time_ns(): int
{
    $usage = Perfidious\Darwin\get_current_thread_resource_usage();
    return $usage->userTimeNs + $usage->systemTimeNs;
}

require __DIR__ . '/thread-cpu-time.inc';
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
