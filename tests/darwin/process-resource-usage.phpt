--TEST--
Perfidious Darwin current-process resource usage exposes cumulative native counters
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-darwin-only.inc'; ?>
--FILE--
<?php

$before = Perfidious\Darwin\get_current_process_resource_usage();
$pages = [];
$accumulator = 0;

for ($attempt = 0; $attempt < 3; $attempt++) {
    $pages[] = str_repeat('x', 32 * 1024 * 1024);

    $deadline = hrtime(true) + 100_000_000;
    while (hrtime(true) < $deadline) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    usleep(10_000);
    $after = Perfidious\Darwin\get_current_process_resource_usage();

    if (
        $after->userTimeNs + $after->systemTimeNs > $before->userTimeNs + $before->systemTimeNs &&
        $after->minorPageFaultCount + $after->majorPageFaultCount >
            $before->minorPageFaultCount + $before->majorPageFaultCount
    ) {
        break;
    }
}

var_dump($before instanceof Perfidious\Darwin\ProcessResourceUsage);
var_dump(array_keys(get_object_vars($before)));

$reflection = new ReflectionClass($before);
$properties = $reflection->getProperties(ReflectionProperty::IS_PUBLIC);
var_dump($reflection->isFinal());
var_dump($reflection->getConstructor()?->isPrivate());
var_dump(array_reduce(
    $properties,
    static fn(bool $readonly, ReflectionProperty $property): bool => $readonly && $property->isReadOnly(),
    true
));

try {
    $before->userTimeNs = 0;
} catch (Error) {
    echo "readonly\n";
}

try {
    $before->extra = 0;
} catch (Error) {
    echo "no dynamic properties\n";
}

var_dump(unserialize(serialize($before)) == $before);
var_dump(array_reduce(
    get_object_vars($before),
    static fn(bool $valid, mixed $value): bool => $valid && is_int($value) && $value >= 0,
    true
));
var_dump($after->userTimeNs + $after->systemTimeNs > $before->userTimeNs + $before->systemTimeNs);
var_dump(
    $after->minorPageFaultCount + $after->majorPageFaultCount >
    $before->minorPageFaultCount + $before->majorPageFaultCount
);
var_dump($after->voluntaryContextSwitchCount + $after->involuntaryContextSwitchCount >=
    $before->voluntaryContextSwitchCount + $before->involuntaryContextSwitchCount);
var_dump(
    (
        $before->instructionCount > 0 &&
        $before->cycleCount > 0 &&
        $after->instructionCount > $before->instructionCount &&
        $after->cycleCount > $before->cycleCount
    ) ||
    (
        $before->instructionCount === 0 && $after->instructionCount === 0 &&
        $before->cycleCount === 0 && $after->cycleCount === 0
    )
);
var_dump(is_int($accumulator));
--EXPECT--
bool(true)
array(8) {
  [0]=>
  string(10) "userTimeNs"
  [1]=>
  string(12) "systemTimeNs"
  [2]=>
  string(19) "minorPageFaultCount"
  [3]=>
  string(19) "majorPageFaultCount"
  [4]=>
  string(27) "voluntaryContextSwitchCount"
  [5]=>
  string(29) "involuntaryContextSwitchCount"
  [6]=>
  string(16) "instructionCount"
  [7]=>
  string(10) "cycleCount"
}
bool(true)
bool(true)
bool(true)
readonly
no dynamic properties
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
