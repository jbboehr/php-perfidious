--TEST--
Perfidious\Handle (struct memory is freed when the handle is closed)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
// warm up so one-time initialization overhead doesn't skew the baseline below
for ($i = 0; $i < 50; $i++) {
    $h = Perfidious\open(["perf::PERF_COUNT_SW_CPU_CLOCK:u"]);
    unset($h);
}

$before = memory_get_usage();

for ($i = 0; $i < 1000; $i++) {
    $h = Perfidious\open(["perf::PERF_COUNT_SW_CPU_CLOCK:u"]);
    unset($h);
}

$after = memory_get_usage();
$delta = $after - $before;

// each leaked handle struct is roughly ~100 bytes; 1000 leaked handles would grow usage
// by well over 60KB, freeing them properly should keep growth far below that
var_dump($delta < 60000);
--EXPECT--
bool(true)
