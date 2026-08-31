<?php

\PHPStan\Testing\assertType('Perfidious\MetricUnit', \Perfidious\Metric::CpuTime->unit());
\PHPStan\Testing\assertType('Perfidious\PmuInfo', \Perfidious\get_pmu_info(0));
\PHPStan\Testing\assertType(
    "Perfidious\\ReadResult<array<int, 'perf::PERF_COUNT_SW_CPU_CLOCK:u'>>",
    \Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u'])->read(),
);
\PHPStan\Testing\assertType(
    'array<string, int>',
    \Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u'])->readArray(),
);
\PHPStan\Testing\assertType(
    'Perfidious\Darwin\ProcessResourceUsage',
    \Perfidious\Darwin\get_current_process_resource_usage(),
);
\PHPStan\Testing\assertType('Perfidious\Windows\ProcessTimes', \Perfidious\Windows\get_current_process_times());
