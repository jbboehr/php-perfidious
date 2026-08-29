--TEST--
Sample subtraction from itself returns a zero delta
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;

$sampler = Sampler::open([Metric::PageFaults, Metric::CpuTime]);
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
