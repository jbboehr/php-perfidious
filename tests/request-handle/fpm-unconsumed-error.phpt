--TEST--
Pending initialization errors are delivered once even after the FPM worker recovers
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
$mode = 'unconsumed-error';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
worker attribution and reuse passed
