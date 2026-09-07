--TEST--
Windows sampler native failures, ownership, CPU-time conversion, and counter wraps
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    die('skip: Unix C compiler harness');
}
if (pack('L', 1) !== pack('V', 1)) {
    die('skip: Windows FILETIME fixture requires little-endian storage');
}
require __DIR__ . '/../native-harness.inc';
perfidious_test_native_skipif();
?>
--FILE--
<?php
require __DIR__ . '/../native-harness.inc';
echo perfidious_test_run_native_harness(
    __DIR__ . '/sampler-harness.c',
    'Windows sampler harness',
    ['-I' . __DIR__ . '/shim'],
);
?>
--EXPECT--
Windows sampler harness passed
