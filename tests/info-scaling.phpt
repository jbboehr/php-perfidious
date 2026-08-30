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
perfidious.global.enable=1
perfidious.global.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php
function deletedTemporaryDescriptors(): array
{
    $descriptors = [];

    foreach (scandir('/proc/self/fd') as $descriptor) {
        $target = @readlink("/proc/self/fd/$descriptor");
        if (is_string($target) && str_contains($target, '(deleted)')) {
            $descriptors[(int) $descriptor] = $target;
        }
    }

    return $descriptors;
}

/** @return resource */
function injectScalingRead(Perfidious\Handle $handle, string $timeEnabled, string $timeRunning)
{
    $before = deletedTemporaryDescriptors();
    $handle->debugInjectScalingRead();
    $after = deletedTemporaryDescriptors();

    $syntheticDescriptor = null;
    foreach ($after as $descriptor => $target) {
        if (($before[$descriptor] ?? null) !== $target) {
            $syntheticDescriptor = $descriptor;
        }
    }

    if ($syntheticDescriptor === null) {
        throw new RuntimeException('Could not find the synthetic read descriptor');
    }

    $stream = fopen("php://fd/$syntheticDescriptor", 'r+');
    if (!is_resource($stream)) {
        throw new RuntimeException('Could not open the synthetic read descriptor');
    }

    $data = stream_get_contents($stream);
    $data = substr_replace($data, $timeEnabled, 8, 8);
    $data = substr_replace($data, $timeRunning, 16, 8);
    rewind($stream);
    if (fwrite($stream, $data) !== strlen($data) || !fflush($stream)) {
        throw new RuntimeException('Could not rewrite the synthetic read');
    }
    rewind($stream);

    return $stream;
}

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

$handle = Perfidious\global_handle();

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
