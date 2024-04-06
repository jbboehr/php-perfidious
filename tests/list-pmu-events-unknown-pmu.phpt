--TEST--
Perfidious\list_pmu_events() - unknown pmu
--EXTENSIONS--
perfidious
--FILE--
<?php
$events = Perfidious\list_pmu_events(PHP_INT_MAX);
--EXPECTF--
%A Uncaught Perfidious\PmuNotFoundException: libpfm: cannot get pmu info for %A