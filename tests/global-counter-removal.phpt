--TEST--
Removed global-counter configuration is inert while retained Linux handles remain usable
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--INI--
perfidious.global.enable=1
perfidious.global.metrics=definitely-not-a-perf-event
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php

$event = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';

var_dump(function_exists('Perfidious\\global_handle'));
var_dump(ini_get('perfidious.global.enable'));
var_dump(ini_get('perfidious.global.metrics'));

$requestHandle = Perfidious\request_handle();
var_dump(array_keys($requestHandle->readArray()));

$ownedHandle = Perfidious\open([$event]);
$ownedHandle->enable();
var_dump(array_keys($ownedHandle->readArray()));

ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();
var_dump(str_contains($info, 'perfidious.global'));
var_dump(str_contains($info, 'Global Metrics'));
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
array(1) {
  [0]=>
  string(31) "perf::PERF_COUNT_SW_CPU_CLOCK:u"
}
bool(false)
bool(false)
