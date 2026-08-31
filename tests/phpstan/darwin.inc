<?php

\PHPStan\Testing\assertType('bool', \Perfidious\DEBUG);
\PHPStan\Testing\assertType('Perfidious\MetricUnit', \Perfidious\Metric::CpuTime->unit());
\PHPStan\Testing\assertType('int', \Perfidious\Darwin\get_current_process_resource_usage()->userTimeNs);
