--TEST--
Perfidious\Handle (open fails with non-string event)
--EXTENSIONS--
perfidious
--FILE--
<?php
$rv = Perfidious\open([1]);
--EXPECTF--
%A Uncaught TypeError: All event names must be strings %A