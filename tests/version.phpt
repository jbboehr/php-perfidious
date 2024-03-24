--TEST--
version
--EXTENSIONS--
perf
--FILE--
<?php
var_dump(PerfExt\VERSION);
--EXPECTF--
string(%d) "%d.%d.%d"