--TEST--
Perfidious Windows current-process APIs return typed results with explicit units
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

$processCycles = Perfidious\Windows\query_current_process_cycle_time();
$threadCycles = Perfidious\Windows\query_current_thread_cycle_time();
$times = Perfidious\Windows\get_current_process_times();
$memory = Perfidious\Windows\get_current_process_memory_info();

var_dump(is_int($processCycles), is_int($threadCycles));
var_dump($times instanceof Perfidious\Windows\ProcessTimes);
var_dump(array_keys(get_object_vars($times)));
var_dump(
    $times->creationTimeFiletime > 0,
    $times->kernelTime100ns >= 0,
    $times->userTime100ns >= 0
);
var_dump($memory instanceof Perfidious\Windows\ProcessMemoryInfo);
var_dump(array_keys(get_object_vars($memory)));
var_dump($memory->pageFaultCount >= 0, $memory->privateUsage >= 0);
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
array(10) {
  [0]=>
  string(14) "pageFaultCount"
  [1]=>
  string(18) "peakWorkingSetSize"
  [2]=>
  string(14) "workingSetSize"
  [3]=>
  string(23) "quotaPeakPagedPoolUsage"
  [4]=>
  string(19) "quotaPagedPoolUsage"
  [5]=>
  string(26) "quotaPeakNonPagedPoolUsage"
  [6]=>
  string(22) "quotaNonPagedPoolUsage"
  [7]=>
  string(13) "pagefileUsage"
  [8]=>
  string(17) "peakPagefileUsage"
  [9]=>
  string(12) "privateUsage"
}
bool(true)
bool(true)
