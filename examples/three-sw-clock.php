<?php

use function Perfidious\open;

$handle = open(["perf::PERF_COUNT_SW_CPU_CLOCK:u"]);
$handle->enable();

for ($i = 0; $i < 3; $i++) {
    var_dump($handle->readArray());
    sleep(1);
}
