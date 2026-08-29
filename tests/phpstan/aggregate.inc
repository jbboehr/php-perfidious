<?php

\PHPStan\Testing\assertType('Perfidious\PmuInfo', \Perfidious\get_pmu_info(0));
\PHPStan\Testing\assertType(
    'Perfidious\Darwin\ProcessResourceUsage',
    \Perfidious\Darwin\get_current_process_resource_usage(),
);
\PHPStan\Testing\assertType('Perfidious\Windows\ProcessTimes', \Perfidious\Windows\get_current_process_times());
