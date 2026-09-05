<?php
require __DIR__ . '/fpm-worker-artifacts.inc';
$fpm = perfidious_test_fpm_path();
$module = perfidious_test_module_path();
$mode ??= 'worker';
if ($fpm === null || $module === null) {
    throw new RuntimeException('FPM test artifacts changed after SKIPIF');
}
$process = proc_open(
    ['python3', __DIR__ . '/fpm-worker.py', $fpm, $module, $opcache, $mode],
    [['pipe', 'r'], ['pipe', 'w'], ['pipe', 'w']],
    $pipes,
);
fclose($pipes[0]);
$output = stream_get_contents($pipes[1]);
$errors = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
if (proc_close($process) !== 0) {
    throw new RuntimeException($output . $errors);
}
echo $mode === 'initialization-error'
    ? "initialization error retry passed\n"
    : "worker attribution and reuse passed\n";
