--TEST--
PerfExt\PmuEnum
--EXTENSIONS--
perf
--FILE--
<?php
$cases = PerfExt\PmuEnum::cases();
var_dump(count($cases) > 0);
var_dump(PerfExt\PmuEnum::PFM_PMU_GEN_IA64);
--EXPECT--
bool(true)
enum(PerfExt\PmuEnum::PFM_PMU_GEN_IA64)