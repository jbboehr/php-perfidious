--TEST--
Sample deltas compose across chronological reads
--EXTENSIONS--
perfidious
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;

$sampler = Sampler::open([Metric::PageFaults, Metric::CpuTime]);
$first = $sampler->read();

$pages = str_repeat('a', 2 * 1024 * 1024);
usleep(1_000);
$second = $sampler->read();

$pages .= str_repeat('b', 2 * 1024 * 1024);
usleep(1_000);
$third = $sampler->read();

$firstToSecond = $second->since($first);
$secondToThird = $third->since($second);
$firstToThird = $third->since($first);

foreach ($sampler->metrics() as $metric) {
    $firstValue = $first->value($metric);
    $secondValue = $second->value($metric);
    $thirdValue = $third->value($metric);

    var_dump($firstValue <= $secondValue && $secondValue <= $thirdValue);
    var_dump(
        $firstToThird->value($metric) ===
            $firstToSecond->value($metric) + $secondToThird->value($metric)
    );
}

var_dump(
    $firstToThird->elapsedTimeNs ===
        $firstToSecond->elapsedTimeNs + $secondToThird->elapsedTimeNs
);
var_dump(strlen($pages) === 4 * 1024 * 1024);

$sampler->close();
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
