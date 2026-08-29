--TEST--
Perfidious Windows current-thread times expose cumulative native CPU accounting
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

$function = new ReflectionFunction('Perfidious\\Windows\\get_current_thread_times');
var_dump($function->getNumberOfParameters() === 0);
var_dump((string) $function->getReturnType() === Perfidious\Windows\ThreadTimes::class);

$processTimes = Perfidious\Windows\get_current_process_times();
$before = Perfidious\Windows\get_current_thread_times();
$accumulator = 0;

for ($attempt = 0; $attempt < 3; $attempt++) {
    for ($iteration = 0; $iteration < 10_000_000; $iteration++) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    $after = Perfidious\Windows\get_current_thread_times();
    $kernelDelta = $after->kernelTime100ns - $before->kernelTime100ns;
    $userDelta = $after->userTime100ns - $before->userTime100ns;
    if ($userDelta > 0 && $userDelta > $kernelDelta) {
        break;
    }
}

var_dump($before instanceof Perfidious\Windows\ThreadTimes);
var_dump(array_keys(get_object_vars($before)));

$reflection = new ReflectionClass($before);
$properties = $reflection->getProperties(ReflectionProperty::IS_PUBLIC);
var_dump($reflection->isFinal());
var_dump($reflection->getConstructor()?->isPrivate());
var_dump(array_reduce(
    $properties,
    static fn(bool $valid, ReflectionProperty $property): bool =>
        $valid && $property->isReadOnly() && (string) $property->getType() === 'int',
    true
));

var_dump($before->creationTimeFiletime > 0);
var_dump($after->creationTimeFiletime === $before->creationTimeFiletime);
var_dump($processTimes->creationTimeFiletime <= $before->creationTimeFiletime);
var_dump($after->kernelTime100ns >= $before->kernelTime100ns);
var_dump($after->userTime100ns > $before->userTime100ns);
var_dump($userDelta > $kernelDelta);
var_dump(is_int($accumulator));
--EXPECT--
bool(true)
bool(true)
bool(true)
array(3) {
  [0]=>
  string(20) "creationTimeFiletime"
  [1]=>
  string(15) "kernelTime100ns"
  [2]=>
  string(13) "userTime100ns"
}
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
