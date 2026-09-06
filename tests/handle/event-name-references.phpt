--TEST--
Handle opening accepts referenced event strings and retains their original names
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php

$expectedName = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';
$event = sprintf('perf::%s:%s', 'PERF_COUNT_SW_CPU_CLOCK', 'u');
$events = [&$event];
$referenceId = ReflectionReference::fromArrayElement($events, 0)->getId();
$handle = Perfidious\open($events);

// Opening must preserve the caller's reference binding. Later assignments
// must not replace the string retained by the already-open handle.
$reference = ReflectionReference::fromArrayElement($events, 0);
var_dump($reference !== null && $reference->getId() === $referenceId);
$events[0] = 'changed through the array';
var_dump($event === 'changed through the array');
$event = 'changed after opening';
var_dump($events[0] === 'changed after opening');
unset($events, $event);
gc_collect_cycles();
for ($i = 0; $i < 1000; $i++) {
    $memoryChurn[] = str_repeat(chr(65 + ($i % 26)), strlen($expectedName));
}
unset($memoryChurn);
var_dump(array_keys($handle->readArray()) === [$expectedName]);
$handle->close();

// Ordinary by-reference iteration leaves referenced elements in the array.
$expectedNames = [$expectedName, 'perf::PERF_COUNT_SW_PAGE_FAULTS:u'];
$events = $expectedNames;
foreach ($events as &$event) {
    $event = sprintf('%s', $event);
}
$referenceId = ReflectionReference::fromArrayElement($events, 1)->getId();
$handle = Perfidious\open($events);
$reference = ReflectionReference::fromArrayElement($events, 1);
var_dump($reference !== null && $reference->getId() === $referenceId);
$event = 'last element changed after opening';
var_dump($events[1] === 'last element changed after opening');
unset($events, $event);
gc_collect_cycles();
for ($i = 0; $i < 1000; $i++) {
    $memoryChurn[] = str_repeat(chr(90 - ($i % 26)), strlen($expectedName));
}
unset($memoryChurn);
var_dump(array_keys($handle->readArray()) === $expectedNames);
$handle->close();
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
