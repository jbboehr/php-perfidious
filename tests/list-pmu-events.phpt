--TEST--
PerfExt\list_pmu_events
--EXTENSIONS--
perf
--FILE--
<?php
$events = PerfExt\list_pmu_events(PerfExt\PmuEnum::PFM_PMU_INTEL_NHM);
var_dump(count($events));
--EXPECT--
int(93)