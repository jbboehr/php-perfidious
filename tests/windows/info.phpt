--TEST--
Windows phpinfo omits Linux-only configuration
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();

var_dump(str_contains($info, 'perfidious'));
var_dump(str_contains($info, 'Version'));
var_dump(str_contains($info, 'perfidious.request.metrics'));
var_dump(str_contains($info, 'perfidious.request.enable'));
var_dump(ini_get('perfidious.overflow_mode'));
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
