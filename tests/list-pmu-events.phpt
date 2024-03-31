--TEST--
PerfExt\list_pmu_events()
--EXTENSIONS--
perf
--FILE--
<?php
$events = PerfExt\list_pmu_events(51);
var_dump(count($events) > 0);
var_dump(count($events));
var_dump(get_class($events[0]));
foreach ($events as $event) {
    if ($event->name === 'perf::PERF_COUNT_HW_CPU_CYCLES') {
        var_dump($event);
    }
}
--EXPECTF--
bool(true)
int(%d)
string(20) "PerfExt\PmuEventInfo"
object(PerfExt\PmuEventInfo)#%d (5) {
  ["name"]=>
  string(30) "perf::PERF_COUNT_HW_CPU_CYCLES"
  ["desc"]=>
  string(24) "PERF_COUNT_HW_CPU_CYCLES"
  ["equiv"]=>
  NULL
  ["pmu"]=>
  int(51)
  ["is_present"]=>
  bool(true)
}