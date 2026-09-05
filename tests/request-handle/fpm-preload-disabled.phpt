--TEST--
Disabled FPM pools release request-counter descriptors inherited from preloading
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/fpm-worker-skipif.inc';
if (preg_match('/^Uid:\s+\d+\s+0\s/m', file_get_contents('/proc/self/status'))) {
    die('skip: master-process preloading requires a non-root user');
}
if (!is_file(getenv('PERFIDIOUS_TEST_OPCACHE') ?: ini_get('extension_dir') . '/opcache.so')) {
    die('skip: opcache shared module is unavailable');
}
?>
--FILE--
<?php
$opcache = getenv('PERFIDIOUS_TEST_OPCACHE') ?: ini_get('extension_dir') . '/opcache.so';
$mode = 'preload-disabled';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
worker attribution and reuse passed
