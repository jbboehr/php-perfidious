--TEST--
version
--EXTENSIONS--
perf
--INI--
perf.enable=1
--FILE--
<?php
var_dump(PerfExt\VERSION);
--EXPECTF--
string(%d) "%d.%d.%d"