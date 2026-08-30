--TEST--
overflow always throws and exposes no configurable modes
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--FILE--
<?php
var_dump(ini_get('perfidious.overflow_mode'));

foreach (['THROW', 'WARN', 'SATURATE', 'WRAP'] as $mode) {
    var_dump(defined("Perfidious\\OVERFLOW_$mode"));
}

$debugFunction = new ReflectionFunction('Perfidious\\debug_uint64_overflow');
var_dump($debugFunction->getNumberOfParameters());
var_dump((new ReflectionMethod(Perfidious\Handle::class, 'read'))->getReturnType()->allowsNull());
var_dump((new ReflectionMethod(Perfidious\Handle::class, 'readArray'))->getReturnType()->allowsNull());

try {
    var_dump(Perfidious\debug_uint64_overflow());
} catch (Perfidious\OverflowException $e) {
    var_dump($e->getMessage());
}

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);
$handle->debugInjectOverflowRead();
try {
    var_dump($handle->read());
} catch (Perfidious\OverflowException $e) {
    var_dump($e->getMessage());
}
--EXPECTF--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
int(0)
bool(false)
bool(false)
string(%d) "value too large: %d > %d"
string(%d) "value too large: %d > %d"
