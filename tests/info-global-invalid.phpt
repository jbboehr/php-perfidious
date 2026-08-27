--TEST--
phpinfo global stats - invalid event name
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--INI--
perfidious.global.enable=1
perfidious.global.metrics=blahblahblah
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%AWarning: PHP Startup: failed to get libpfm event encoding for blahblahblah: event not found%A