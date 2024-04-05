--TEST--
Perfidious\list_pmus()
--EXTENSIONS--
perfidious
--FILE--
<?php
$pmus = Perfidious\list_pmus();
var_dump(count($pmus) > 0);
var_dump(get_class($pmus[0]));
foreach ($pmus as $pmu) {
    if ($pmu->name === 'perf') {
        var_dump($pmu);
    }
}
--EXPECTF--
bool(true)
string(%d) "Perfidious\PmuInfo"
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