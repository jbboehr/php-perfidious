<?php
if (PHP_OS_FAMILY === 'Linux') {
    try {
        $probe = Perfidious\Sampler::open([Perfidious\Metric::CpuTime]);
        $probe->close();
    } catch (Perfidious\IOException $error) {
        if (in_array($error->getCode(), [1, 13], true)) {
            die('skip: kernel-inclusive perf counters are not permitted');
        }
        throw $error;
    }
}
