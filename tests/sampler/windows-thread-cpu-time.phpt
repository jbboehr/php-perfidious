--TEST--
Sampler reads current-thread CPU time on Windows
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

function native_thread_cpu_time_ns(): int
{
    $times = Perfidious\Windows\get_current_thread_times();
    return ($times->kernelTime100ns + $times->userTime100ns) * 100;
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
