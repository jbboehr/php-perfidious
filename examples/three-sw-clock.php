<?php

$handle = PerfExt\open(["perf::PERF_COUNT_SW_CPU_CLOCK"]);
$handle->enable();

for ($i = 0; $i < 3; $i++) {
    var_dump($handle->readArray());
    sleep(1);
}

