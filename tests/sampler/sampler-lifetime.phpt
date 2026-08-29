--TEST--
Samples and deltas outlive their sampler and one another
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;

$metrics = [Metric::PageFaults, Metric::CpuTime];
$sampler = Sampler::open($metrics);
$before = $sampler->read();
usleep(1_000);
$after = $sampler->read();

$expected = [];
foreach ($metrics as $metric) {
    $expected[$metric->value] = $after->value($metric) - $before->value($metric);
}

$sampler->close();
unset($sampler);
gc_collect_cycles();

foreach ($metrics as $metric) {
    var_dump(is_int($before->value($metric)));
    var_dump(is_int($after->value($metric)));
}

$delta = $after->since($before);
unset($before, $after);
gc_collect_cycles();

foreach ($metrics as $metric) {
    var_dump($delta->value($metric) === $expected[$metric->value]);
}
var_dump($delta->elapsedTimeNs > 0);

$destructorSequencesValid = true;
for ($i = 0; $i < 128; $i++) {
    $unclosedSampler = Sampler::open([Metric::CpuTime]);
    $retainedSample = $unclosedSampler->read();
    unset($unclosedSampler);

    $destructorSequencesValid =
        $destructorSequencesValid && is_int($retainedSample->value(Metric::CpuTime));
    unset($retainedSample);
}
gc_collect_cycles();
var_dump($destructorSequencesValid);
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
