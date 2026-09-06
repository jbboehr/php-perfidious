--TEST--
Windows sampler native failures, ownership, CPU-time conversion, and counter wraps
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    die('skip: Unix C compiler harness');
}
if (pack('L', 1) !== pack('V', 1)) {
    die('skip: Windows FILETIME fixture requires little-endian storage');
}
if (!function_exists('proc_open')) {
    die('skip: process execution is unavailable');
}
foreach (['cc', 'php-config'] as $name) {
    $found = false;
    $testPhp = getenv('TEST_PHP_EXECUTABLE');
    $directories = explode(PATH_SEPARATOR, (string) getenv('PATH'));
    array_unshift($directories, dirname(is_string($testPhp) && $testPhp !== '' ? $testPhp : PHP_BINARY));
    foreach ($directories as $directory) {
        if (is_executable(rtrim($directory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name)) {
            $found = true;
            break;
        }
    }
    if (!$found) {
        die('skip: ' . $name . ' is unavailable');
    }
}
?>
--FILE--
<?php
function runSamplerCommand(array $command): array
{
    $environment = getenv();
    if (!is_array($environment)) {
        $environment = [];
    }
    unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);
    $process = proc_open($command, [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes, null, $environment);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start sampler harness command');
    }
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    return [proc_close($process), $output];
}

$testPhp = getenv('TEST_PHP_EXECUTABLE');
$directories = explode(PATH_SEPARATOR, (string) getenv('PATH'));
array_unshift($directories, dirname(is_string($testPhp) && $testPhp !== '' ? $testPhp : PHP_BINARY));
$executables = [];
foreach (['cc', 'php-config'] as $name) {
    foreach ($directories as $directory) {
        $candidate = rtrim($directory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name;
        if (is_executable($candidate)) {
            $executables[$name] = $candidate;
            break;
        }
    }
    if (!isset($executables[$name])) {
        throw new RuntimeException($name . ' disappeared after SKIPIF');
    }
}

[$includeStatus, $includeFlags] = runSamplerCommand([$executables['php-config'], '--includes']);
if ($includeStatus !== 0) {
    throw new RuntimeException('php-config failed: ' . $includeFlags);
}
$root = dirname(__DIR__, 2);
$binary = sprintf('%s/perfidious-windows-sampler-%d-%s', sys_get_temp_dir(), getmypid(), bin2hex(random_bytes(8)));
register_shutdown_function(static function () use ($binary): void {
    @unlink($binary);
});
$command = [
    $executables['cc'],
    '-D_GNU_SOURCE',
    '-std=c11',
    '-Wall',
    '-Wextra',
    '-Werror',
    ...preg_split('/\s+/', trim($includeFlags)),
    '-I' . $root,
    '-I' . __DIR__ . '/shim',
    __DIR__ . '/sampler-harness.c',
    '-o',
    $binary,
];
[$compileStatus, $compileOutput] = runSamplerCommand($command);
if ($compileStatus !== 0) {
    throw new RuntimeException('Windows sampler harness did not compile: ' . $compileOutput);
}
[$status, $output] = runSamplerCommand([$binary]);
if ($status !== 0) {
    throw new RuntimeException('Windows sampler harness failed: ' . $output);
}
echo $output;
?>
--EXPECT--
Windows sampler harness passed
