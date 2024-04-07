--TEST--
phpinfo
--EXTENSIONS--
perfidious
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%A
perfidious
%A
Version => %A
Released => %A
Authors => %A
%A
Directive => Local Value => Master Value
perfidious.global.enable => 0 => 0
perfidious.global.metrics => %s => %s
perfidious.overflow_mode => 0 => 0
perfidious.request.enable => 0 => 0
perfidious.request.metrics => %s => %s
%A
