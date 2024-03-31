--TEST--
PerfExt\list_pmus
--EXTENSIONS--
perf
--FILE--
<?php
$pmus = PerfExt\list_pmus();
var_dump(count($pmus) > 0);
--EXPECT--
bool(true)