--TEST--
phpinfo per-request stats - invalid event name
--EXTENSIONS--
perfidious
--INI--
perfidious.request.enable=1
perfidious.request.metrics=blahblahblah
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%AWarning: PHP Startup: failed to get libpfm event encoding for blahblahblah: event not found%A
