--TEST--
phpinfo scales counters without overflowing an intermediate uint64_t
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
<?php if (!Perfidious\DEBUG) die('skip: must be compiled in debug mode'); ?>
<?php if (PHP_INT_SIZE !== 8) die('skip: requires 64-bit PHP integers'); ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php
require __DIR__ . '/inject-scaling-read.inc';

function metricRow(): string
{
    ob_start();
    phpinfo(INFO_MODULES);
    $info = ob_get_clean();

    foreach (explode("\n", $info) as $line) {
        if (str_starts_with($line, 'perf::PERF_COUNT_SW_CPU_CLOCK:u =>')) {
            return $line;
        }
    }

    throw new RuntimeException('Could not find the phpinfo metric row');
}

$handle = Perfidious\request_handle();

$handle->debugInjectScalingRead();
echo "intermediate overflow: ", metricRow(), "\n";

$uint64Max = str_repeat("\xff", 8);
$one = pack('Q', 1);
$zero = str_repeat("\0", 8);

$stream = injectScalingRead($handle, $uint64Max, $one);
echo "final overflow: ", metricRow(), "\n";
fclose($stream);

$stream = injectScalingRead($handle, $uint64Max, $zero);
echo "not running: ", metricRow(), "\n";
fclose($stream);
--EXPECT--
intermediate overflow: perf::PERF_COUNT_SW_CPU_CLOCK:u => 9223372036854775807 => 9223372036854775807 => 100%
final overflow: perf::PERF_COUNT_SW_CPU_CLOCK:u => 9223372036854775807 => overflow => 0%
not running: perf::PERF_COUNT_SW_CPU_CLOCK:u => 9223372036854775807 => 0 => 0%
