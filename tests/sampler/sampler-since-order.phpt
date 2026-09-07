--TEST--
Sample subtraction from itself returns a zero delta
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/skipif-perf-permissions.inc';
?>
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;

$scope = PHP_OS_FAMILY === 'Linux' ? Scope::CurrentThread : Scope::CurrentProcess;

$sampler = Sampler::open([Metric::PageFaults, Metric::CpuTime], $scope);
$sample = $sampler->read();
$delta = $sample->since($sample);

var_dump($delta->elapsedTimeNs);
foreach ($sampler->metrics() as $metric) {
    var_dump($delta->value($metric));
}

$sampler->close();
--EXPECT--
int(0)
int(0)
int(0)
