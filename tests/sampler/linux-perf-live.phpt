--TEST--
Live user-only perf counters and thread isolation
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
    __DIR__ . '/linux-perf-live.c',
    'Linux perf sampler harness',
    ['-pthread', '-Wno-unused-function'],
);
?>
--EXPECT--
Live user-only perf counters and thread isolation passed
