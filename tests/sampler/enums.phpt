--TEST--
Sampler metric and scope enums expose stable configuration values
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Scope;

var_dump(array_map(
    static fn(Metric $metric): string => $metric->name . '=' . $metric->value,
    Metric::cases(),
));
var_dump(Metric::from('cpu-time') === Metric::CpuTime);
var_dump(Metric::from('page-faults') === Metric::PageFaults);

var_dump(array_map(
    static fn(Scope $scope): string => $scope->name . '=' . $scope->value,
    Scope::cases(),
));
var_dump(Scope::from('current-process') === Scope::CurrentProcess);
--EXPECT--
array(5) {
  [0]=>
  string(16) "CpuTime=cpu-time"
  [1]=>
  string(22) "PageFaults=page-faults"
  [2]=>
  string(32) "ContextSwitches=context-switches"
  [3]=>
  string(20) "CpuCycles=cpu-cycles"
  [4]=>
  string(25) "Instructions=instructions"
}
bool(true)
bool(true)
array(2) {
  [0]=>
  string(30) "CurrentProcess=current-process"
  [1]=>
  string(28) "CurrentThread=current-thread"
}
bool(true)
