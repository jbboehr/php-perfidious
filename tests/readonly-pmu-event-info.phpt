--TEST--
Perfidious\PmuEventInfo (readonly / no dynamic properties)
--EXTENSIONS--
perfidious
--FILE--
<?php
$idx = Perfidious\list_pmu_events(51)[0]->idx;
$info = Perfidious\get_pmu_event_info(51, $idx);
try {
    $info->name = "x";
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $info->foo = "x";
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
Cannot modify readonly property Perfidious\PmuEventInfo::$name
Cannot create dynamic property Perfidious\PmuEventInfo::$foo
