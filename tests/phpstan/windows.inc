<?php

\PHPStan\Testing\assertType('bool', \Perfidious\DEBUG);
\PHPStan\Testing\assertType('Perfidious\MetricUnit', \Perfidious\Metric::CpuTime->unit());
\PHPStan\Testing\assertType('int', \Perfidious\Windows\get_current_process_times()->userTime100ns);
