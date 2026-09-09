--TEST--
Perfidious Windows process creation precedes current-thread creation
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-windows-only.inc';
perfidious_skip_if_wine('native process/thread creation order');
?>
--FILE--
<?php

$process = Perfidious\Windows\get_current_process_times();
$thread = Perfidious\Windows\get_current_thread_times();
var_dump($process->creationTimeFiletime <= $thread->creationTimeFiletime);
--EXPECT--
bool(true)
