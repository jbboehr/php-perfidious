--TEST--
Request counters are reopened in FPM workers after master-process preloading
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/fpm-worker-skipif.inc';
// /proc/self/status avoids depending on the optional posix extension.
if (preg_match('/^Uid:\s+\d+\s+0\s/m', file_get_contents('/proc/self/status'))) {
    die('skip: master-process preloading requires a non-root user');
}
if (perfidious_test_opcache_path() === null) {
    die('skip: opcache is unavailable');
}
?>
--FILE--
<?php
$mode = 'preload';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
worker attribution and reuse passed
