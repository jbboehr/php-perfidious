--TEST--
Perfidious Windows thread profiling validates the hardware counter mask
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

foreach ([-1, 65536] as $hardwareCounters) {
    try {
        Perfidious\Windows\enable_current_thread_profiling(hardwareCounterMask: $hardwareCounters);
    } catch (ValueError $e) {
        echo "invalid mask\n";
    }
}
--EXPECT--
invalid mask
invalid mask
