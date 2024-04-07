--TEST--
Perfidious\VERSION
--EXTENSIONS--
perfidious
--FILE--
<?php
var_dump(Perfidious\VERSION);
--EXPECTF--
string(%d) "%d.%d.%d"