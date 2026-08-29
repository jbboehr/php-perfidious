--TEST--
Sampler validates referenced array values by their PHP value
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;

$metric = Metric::CpuTime;
$metrics = [&$metric];

var_dump($metrics[0] instanceof Metric);
$sampler = Sampler::open($metrics);
var_dump($sampler->metrics() === [Metric::CpuTime]);
$sampler->close();

try {
    Sampler::open([&$metric, Metric::CpuTime]);
} catch (ValueError) {
    echo "referenced duplicate rejected\n";
}

$invalid = 'cpu-time';
try {
    Sampler::open([&$invalid]);
} catch (TypeError) {
    echo "invalid referenced value rejected\n";
}
--EXPECT--
bool(true)
bool(true)
referenced duplicate rejected
invalid referenced value rejected
