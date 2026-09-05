--TEST--
Darwin module loads with only Darwin platform-specific APIs
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-darwin-only.inc'; ?>
--FILE--
<?php

ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();

var_dump(defined('Perfidious\\VERSION'));
var_dump(class_exists('Perfidious\\ReadResult'));
var_dump(interface_exists('Perfidious\\ExceptionInterface'));
var_dump(is_subclass_of('Perfidious\\IOException', 'Perfidious\\ExceptionInterface'));
var_dump(is_subclass_of('Perfidious\\OverflowException', 'Perfidious\\ExceptionInterface'));
var_dump(str_contains($info, 'perfidious'));
var_dump(str_contains($info, 'Version'));
var_dump(str_contains($info, 'perfidious.request.enable'));
var_dump(function_exists('Perfidious\\open'));
var_dump(function_exists('Perfidious\\Darwin\\get_current_process_resource_usage'));
var_dump(function_exists('Perfidious\\Darwin\\get_current_thread_resource_usage'));
var_dump(function_exists('Perfidious\\Windows\\query_current_process_cycle_time'));
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
