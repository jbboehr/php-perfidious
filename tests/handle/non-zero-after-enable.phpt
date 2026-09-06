--TEST--
Perfidious\Handle (non-zero after enable)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$event = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';
$handle = Perfidious\open([$event]);
$handle->enable();
$deadline = hrtime(true) + 5_000_000;
while (hrtime(true) < $deadline) {
    hash('sha256', 'non-zero counter');
}
$values = $handle->readArray();
$value = $values[$event] ?? null;
var_dump(is_int($value) && $value > 0);
$handle->close();
--EXPECT--
bool(true)
