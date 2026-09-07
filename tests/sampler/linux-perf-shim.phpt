--TEST--
Linux perf sampler mapping, scaling, failures and ownership
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Linux') {
    die('skip: Linux perf compiler harness');
}
require __DIR__ . '/../native-harness.inc';
perfidious_test_native_skipif();
?>
--FILE--
<?php
require __DIR__ . '/../native-harness.inc';
echo perfidious_test_run_native_harness(
    __DIR__ . '/linux-perf-harness.c',
    'Linux perf sampler harness',
    ['-Wno-unused-function'],
);
?>
--EXPECT--
Linux perf sampler mapping, scaling, failures and ownership passed
