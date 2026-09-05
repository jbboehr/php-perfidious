--TEST--
Request counters measure each FPM worker and reset between requests
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/fpm-worker-skipif.inc';
?>
--FILE--
<?php
$opcache = '';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
worker attribution and reuse passed
