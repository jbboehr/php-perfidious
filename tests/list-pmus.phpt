--TEST--
PerfExt\list_pmus()
--EXTENSIONS--
perf
--FILE--
<?php
$pmus = PerfExt\list_pmus();
var_dump(count($pmus) > 0);
var_dump(get_class($pmus[0]));
foreach ($pmus as $pmu) {
    if ($pmu->name === 'perf') {
        var_dump($pmu);
    }
}
--EXPECTF--
bool(true)
string(15) "PerfExt\PmuInfo"
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