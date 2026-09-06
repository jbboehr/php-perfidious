--TEST--
Oversized request metrics defer once, retry, and preserve FPM handle reuse
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
$mode = 'metric-limit';
require __DIR__ . '/fpm-worker-run.inc';
?>
--EXPECT--
metric limit retry and reuse passed
