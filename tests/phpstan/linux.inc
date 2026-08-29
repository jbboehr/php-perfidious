<?php

\PHPStan\Testing\assertType('bool', \Perfidious\DEBUG);
\PHPStan\Testing\assertType('int', \Perfidious\get_pmu_event_info(0, 0)->idx);
