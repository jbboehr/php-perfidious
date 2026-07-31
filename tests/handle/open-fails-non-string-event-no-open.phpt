--TEST--
Perfidious\Handle (open bails out immediately on a non-string event name, without opening anything)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
$before = Perfidious\debug_get_open_ex_call_count();

try {
    // a valid event name before the bad one, so a buggy implementation would have real work
    // to do (and something to leak) by the time it hits the invalid element
    Perfidious\open(["perf::PERF_COUNT_SW_CPU_CLOCK:u", 1]);
    echo "FAIL: no exception thrown\n";
} catch (\TypeError $e) {
    var_dump($e->getMessage());
}

$after = Perfidious\debug_get_open_ex_call_count();
var_dump($after === $before);
--EXPECT--
string(31) "All event names must be strings"
bool(true)
