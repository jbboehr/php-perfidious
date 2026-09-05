--TEST--
phpinfo per-request stats - invalid event name
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=blahblahblah
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();
var_dump(str_contains($info, 'perfidious.request.metrics'));
var_dump(str_contains($info, 'Request Metrics'));
var_dump(str_contains($info, 'Warning:'));
echo "request completed\n";
--EXPECT--
bool(true)
bool(false)
bool(false)
request completed
