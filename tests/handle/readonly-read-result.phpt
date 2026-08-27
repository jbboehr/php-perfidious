--TEST--
Perfidious\ReadResult (readonly / no dynamic properties)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$h = Perfidious\open([
    "perf::PERF_COUNT_SW_CPU_CLOCK:u",
]);
$h->enable();
$result = $h->read();
try {
    $result->timeEnabled = 1;
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $result->foo = 1;
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
Cannot modify readonly property Perfidious\ReadResult::$timeEnabled
Cannot create dynamic property Perfidious\ReadResult::$foo
