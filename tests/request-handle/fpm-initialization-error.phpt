--TEST--
Request counter initialization errors preserve details and retry in an FPM worker
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
$mode = 'initialization-error';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
initialization error retry passed
