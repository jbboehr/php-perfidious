--TEST--
Perfidious\Handle retains dynamic event names after caller values are destroyed
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php

$expectedName = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';
$eventName = sprintf('perf::%s:%s', 'PERF_COUNT_SW_CPU_CLOCK', 'u');
$eventNames = [$eventName];
$handle = Perfidious\open($eventNames);

unset($eventNames, $eventName);
for ($i = 0; $i < 1000; $i++) {
    $memoryChurn[] = str_repeat(chr(65 + ($i % 26)), strlen($expectedName));
}
unset($memoryChurn);
gc_collect_cycles();

$handle->enable();
foreach (range(1, 3) as $_) {
    $values = $handle->readArray();
    var_dump(array_keys($values) === [$expectedName]);
    var_dump(is_int($values[$expectedName] ?? null));
}
$handle->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
