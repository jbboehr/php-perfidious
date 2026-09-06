--TEST--
Referenced non-string events are rejected before native handle acquisition
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php

$stringable = new class {
    public function __toString(): string
    {
        throw new LogicException('Event values must not be coerced to strings');
    }
};
$resource = fopen('php://memory', 'r');
$before = Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null;
foreach ([null, false, 123, 1.25, [], $resource, $stringable] as $invalid) {
    $events = ['perf::PERF_COUNT_SW_CPU_CLOCK:u', &$invalid];
    $referenceId = ReflectionReference::fromArrayElement($events, 1)->getId();
    try {
        Perfidious\open($events);
        echo "non-string event was accepted\n";
    } catch (TypeError $error) {
        if ($error->getMessage() !== 'All event names must be strings') {
            echo "unexpected type error: ", $error->getMessage(), "\n";
        }
    }
    $reference = ReflectionReference::fromArrayElement($events, 1);
    if ($reference === null || $reference->getId() !== $referenceId) {
        echo "invalid event reference was replaced\n";
    }
    $events[1] = 'changed through the array';
    if ($invalid !== 'changed through the array') {
        echo "invalid event reference binding was broken\n";
    }
}
fclose($resource);
if ($before !== null && Perfidious\debug_get_open_ex_call_count() !== $before) {
    echo "invalid event list reached native acquisition\n";
}
echo "referenced non-string values rejected without coercion\n";
?>
--EXPECT--
referenced non-string values rejected without coercion
