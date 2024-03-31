--TEST--
info
--EXTENSIONS--
perf
--FILE--
<?php
phpinfo(INFO_MODULES);
--EXPECTF--
%A
perf
%A
Version => %A
Released => %A
Authors => %A
%A
Directive => Local Value => Master Value
perf.global.enable => 0 => 0
perf.global.metrics => %s => %s
perf.request.enable => 0 => 0
perf.request.metrics => %s => %s
%A
