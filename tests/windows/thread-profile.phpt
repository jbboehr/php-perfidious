--TEST--
Perfidious Windows thread profiling lifecycle, dispatch counters, and result objects
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-windows-only.inc';
perfidious_skip_if_wine('EnableThreadProfiling');
?>
--FILE--
<?php

$profile = Perfidious\Windows\enable_current_thread_profiling();
$before = $profile->read();
$after = $before;

for ($attempt = 0; $attempt < 3; $attempt++) {
    for ($i = 0; $i < 8; $i++) {
        usleep(1000);
    }

    $deadline = hrtime(true) + 50_000_000;
    while (hrtime(true) < $deadline) {
    }

    $after = $profile->read();
    if (
        $after->contextSwitchCount > $before->contextSwitchCount &&
        $after->cycleCount > $before->cycleCount
    ) {
        break;
    }
}

var_dump($profile instanceof Perfidious\Windows\ThreadProfile);
var_dump($after instanceof Perfidious\Windows\ThreadProfileSnapshot);
var_dump(array_keys(get_object_vars($after)));
var_dump(
    strlen($after->waitReasonBitmapHex) === 18 &&
    str_starts_with($after->waitReasonBitmapHex, '0x')
);
var_dump($after->contextSwitchCount > $before->contextSwitchCount);
var_dump($after->cycleCount > $before->cycleCount);
var_dump($after->hardwareCounters);

$reflection = new ReflectionClass($after);
$properties = $reflection->getProperties(ReflectionProperty::IS_PUBLIC);
var_dump($reflection->isFinal());
var_dump($reflection->getConstructor()?->isPrivate());
var_dump(array_reduce(
    $properties,
    static fn(bool $readonly, ReflectionProperty $property): bool => $readonly && $property->isReadOnly(),
    true
));

$property = $properties[0]->getName();
try {
    $after->$property = 0;
} catch (Error) {
    echo "readonly\n";
}
try {
    $after->extra = 0;
} catch (Error) {
    echo "no dynamic properties\n";
}
var_dump(unserialize(serialize($after)) == $after);

$profile->close();
$profile->close();

try {
    $profile->read();
} catch (Perfidious\ClosedException $e) {
    echo "closed\n";
}
--EXPECT--
bool(true)
bool(true)
array(6) {
  [0]=>
  string(18) "contextSwitchCount"
  [1]=>
  string(19) "waitReasonBitmapHex"
  [2]=>
  string(10) "cycleCount"
  [3]=>
  string(14) "readRetryCount"
  [4]=>
  string(20) "hardwareCounterCount"
  [5]=>
  string(16) "hardwareCounters"
}
bool(true)
bool(true)
bool(true)
array(0) {
}
bool(true)
bool(true)
bool(true)
readonly
no dynamic properties
bool(true)
closed
