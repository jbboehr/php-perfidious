--TEST--
PerfExt\get_pmu_info()
--EXTENSIONS--
perf
--FILE--
<?php
$pmu = PerfExt\get_pmu_info(51);
var_dump($pmu);
try {
    PerfExt\get_pmu_info(0);
} catch (\Throwable $e) {
    var_dump(get_class($e));
}
--EXPECTF--
object(PerfExt\PmuInfo)#%d (6) {
  ["name"]=>
  string(4) "perf"
  ["desc"]=>
  string(23) "perf_events generic PMU"
  ["pmu"]=>
  int(51)
  ["type"]=>
  int(3)
  ["nevents"]=>
  int(%d)
  ["is_present"]=>
  bool(true)
}
string(28) "PerfExt\PmuNotFoundException"