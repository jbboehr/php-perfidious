--TEST--
Perfidious\get_pmu_event_info()
--EXTENSIONS--
perfidious
--FILE--
<?php
$idx = Perfidious\list_pmu_events(51)[0]->idx;
$pmu = Perfidious\get_pmu_event_info(51, $idx);
var_dump(get_class($pmu));
try {
    Perfidious\get_pmu_event_info(0, $idx);
} catch (\Throwable $e) {
    var_dump(get_class($e), $e->getMessage());
}
try {
    Perfidious\get_pmu_event_info(51, 12345);
} catch (\Throwable $e) {
    var_dump(get_class($e), $e->getMessage());
}
--EXPECTF--
string(%d) "Perfidious\PmuEventInfo"
string(%d) "Perfidious\PmuNotFoundException"
string(%d) "cannot get pmu info for %d: not supported"
string(%d) "Perfidious\PmuEventNotFoundException"
string(%d) "libpfm: cannot get event info for %d: invalid parameters"