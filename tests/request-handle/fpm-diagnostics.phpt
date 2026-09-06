--TEST--
FPM fixture rejects child sanitizer reports and unsuccessful shutdown
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/fpm-worker-skipif.inc';
?>
--FILE--
<?php
require __DIR__ . '/fpm-worker-artifacts.inc';
$process = proc_open(
    ['python3', __DIR__ . '/fpm-diagnostics.py', perfidious_test_fpm_path(), perfidious_test_fpm_module_path()],
    [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]],
    $pipes,
);
fclose($pipes[0]);
$output = stream_get_contents($pipes[1]);
fclose($pipes[1]);
if (proc_close($process) !== 0) {
    throw new RuntimeException($output);
}
echo "FPM diagnostic checks passed\n";
?>
--EXPECT--
FPM diagnostic checks passed
