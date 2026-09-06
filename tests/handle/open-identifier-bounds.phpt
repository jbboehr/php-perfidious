--TEST--
Handle opening validates PID and CPU integers before native acquisition
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php

function checkOpenError(int $pid, int $cpu, string $expected, ?string $message = null): void
{
    try {
        // The invalid event stops representable arguments before any native acquisition.
        Perfidious\open([null], pid: $pid, cpu: $cpu);
        echo "opening unexpectedly succeeded\n";
    } catch (Throwable $error) {
        if (get_class($error) !== $expected) {
            printf("pid=%d cpu=%d: expected %s, got %s\n", $pid, $cpu, $expected, get_class($error));
        }
        if ($message !== null && !str_contains($error->getMessage(), $message)) {
            printf("diagnostic missing %s: %s\n", $message, $error->getMessage());
        }
    }
}

$before = Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null;

// A CPU ID is not bounded by the number of online CPUs. Even INT_MAX must
// reach event validation, leaving CPU existence and availability to the kernel.
foreach ([-1, 0, 1, 2147483647] as $cpu) {
    checkOpenError(0, $cpu, TypeError::class, 'All event names must be strings');
}
foreach ([-2, PHP_INT_MIN] as $cpu) {
    checkOpenError(0, $cpu, ValueError::class, '($cpu)');
}

// Representable negative PIDs retain their existing native validation path.
foreach ([-2147483648, -2, -1, 0] as $pid) {
    checkOpenError($pid, -1, TypeError::class, 'All event names must be strings');
}
// A debug build bypasses capability checks, so it can also prove that the
// positive endpoint remains representable without reaching native acquisition.
if (Perfidious\DEBUG) {
    checkOpenError(2147483647, -1, TypeError::class, 'All event names must be strings');
}
if (PHP_INT_SIZE > 4) {
    // Whole 32-bit-width offsets catch validation performed after narrowing,
    // where these values would otherwise alias representable native PIDs.
    foreach ([PHP_INT_MIN, -4294967296, -2147483649, 2147483648, 4294967296, PHP_INT_MAX] as $pid) {
        checkOpenError($pid, -1, Perfidious\OverflowException::class, (string) $pid);
    }
    foreach ([2147483648, 4294967296, PHP_INT_MAX] as $cpu) {
        checkOpenError(0, $cpu, Perfidious\OverflowException::class, (string) $cpu);
    }
}

if ($before !== null && Perfidious\debug_get_open_ex_call_count() !== $before) {
    echo "invalid arguments reached native acquisition\n";
}
echo "identifier bounds checked before native acquisition\n";
?>
--EXPECT--
identifier bounds checked before native acquisition
