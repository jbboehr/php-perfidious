<?php

\PHPStan\Testing\assertType('bool', \Perfidious\DEBUG);
\PHPStan\Testing\assertType('int', \Perfidious\get_pmu_event_info(0, 0)->idx);
\PHPStan\Testing\assertType(
    "Perfidious\\ReadResult<array<int, 'perf::PERF_COUNT_SW_CPU_CLOCK:u'>>|null",
    \Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u'])->read(),
);
\PHPStan\Testing\assertType(
    'array<string, int>|null',
    \Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u'])->readArray(),
);
