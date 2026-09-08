<?php

declare(strict_types=1);

if (
    PHP_OS_FAMILY !== 'Linux' || PHP_INT_SIZE !== 8 || PHP_ZTS || PHP_DEBUG
    || PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION !== getenv('EXPECTED_PHP_VERSION')
    || phpversion('perfidious') !== getenv('EXPECTED_VERSION')
) {
    throw new RuntimeException('Installed release or PHP build does not match the Linux package');
}

// This also checks that the statically linked libpfm initialized successfully without perf permissions.
if (Perfidious\list_pmus() === []) {
    throw new RuntimeException('The packaged module did not expose any PMUs');
}

try {
    $sampler = Perfidious\Sampler::open([Perfidious\Metric::CpuTime]);
    $sampler->read();
    $sampler->close();
    echo "CPU-time sampler: passed\n";
} catch (Perfidious\IOException $error) {
    if (!in_array($error->getCode(), [1, 13], true)) {
        throw $error;
    }
    echo "CPU-time sampler: skipped (perf permission denied)\n";
}

echo "Linux release module: passed\n";
