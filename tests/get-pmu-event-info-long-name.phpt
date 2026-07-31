--TEST--
Perfidious\PmuEventInfo (name is truncated, not over-read, when pmu::event exceeds the internal buffer)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
$pmu_name = str_repeat("A", 300);
$event_name = str_repeat("B", 300);

$info = Perfidious\debug_pmu_event_info_from_names($pmu_name, $event_name);

$expected = substr($pmu_name . "::" . $event_name, 0, 511);

var_dump(strlen($info->name) === 511);
var_dump($info->name === $expected);
--EXPECT--
bool(true)
bool(true)
