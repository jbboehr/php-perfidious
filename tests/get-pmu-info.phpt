--TEST--
Perfidious\get_pmu_info()
--EXTENSIONS--
perfidious
--FILE--
<?php
$pmu = Perfidious\get_pmu_info(51);
var_dump($pmu);
try {
    Perfidious\get_pmu_info(0);
} catch (\Throwable $e) {
    var_dump(get_class($e));
}
--EXPECTF--
object(Perfidious\PmuInfo)#%d (6) {
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
string(%d) "Perfidious\PmuNotFoundException"