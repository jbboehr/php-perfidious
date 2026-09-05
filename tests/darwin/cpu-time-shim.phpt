--TEST--
Darwin process APIs convert Mach CPU time to nanoseconds with checked arithmetic
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Linux') {
    die('skip: Linux build of the Darwin backend with substitute native calls');
}
if (PHP_INT_SIZE !== 8) {
    die('skip: Darwin integer-boundary fixtures require 64-bit PHP');
}
if (!function_exists('proc_open')) {
    die('skip: process execution is unavailable');
}
foreach (['cc', 'php-config'] as $tool) {
    $found = false;
    foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $directory) {
        $found = $found || is_executable($directory . '/' . $tool);
    }
    if (!$found) {
        die('skip: ' . $tool . ' is unavailable');
    }
}
$environment = getenv();
unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);
$process = proc_open(
    [PHP_BINARY, '-n', '-r', 'echo extension_loaded("perfidious") ? "static" : "shared";'],
    [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]],
    $pipes,
    null,
    $environment,
);
if (!is_resource($process)) {
    throw new RuntimeException('Could not check static extension availability');
}
fclose($pipes[0]);
$output = stream_get_contents($pipes[1]);
fclose($pipes[1]);
if (proc_close($process) !== 0) {
    throw new RuntimeException($output);
}
if ($output === 'static') {
    die('skip: the isolated Darwin module requires PHP without a built-in perfidious extension');
}
?>
--FILE--
<?php

function runCpuTimeCommand(array $command, array $settings = []): string
{
    $environment = getenv();
    unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);
    foreach (array_keys($environment) as $key) {
        if (str_starts_with($key, 'PERFIDIOUS_TEST_')) {
            unset($environment[$key]);
        }
    }
    $environment = $settings + $environment;
    $process = proc_open($command, [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes, null, $environment);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start CPU-time test command');
    }
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    if (proc_close($process) !== 0) {
        throw new RuntimeException($output);
    }
    return $output;
}

$root = dirname(__DIR__, 2);
$module = tempnam(sys_get_temp_dir(), 'perfidious-darwin-cpu-');
register_shutdown_function(static function () use ($module): void {
    @unlink($module);
});
$includes = preg_split('/\s+/', trim(runCpuTimeCommand(['php-config', '--includes'])));
runCpuTimeCommand([
    'cc', '-D_GNU_SOURCE', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    '-shared', '-fPIC', '-DPERFIDIOUS_PLATFORM_DARWIN=1', '-DCOMPILE_DL_PERFIDIOUS=1',
    '-Ddlsym=perfidious_test_dlsym', ...$includes, '-I' . $root, '-I' . __DIR__ . '/shim',
    $root . '/src/extension.c', $root . '/src/exceptions.c', $root . '/src/read_result.c',
    $root . '/src/sampler.c', $root . '/src/platform.c',
    $root . '/src/darwin/functions.c', $root . '/src/darwin/sampler.c',
    __DIR__ . '/cpu-time-native-shim.c', '-o', $module,
]);
foreach (['125/3', '1/1', '2/3', '3/2', '0/3', '125/0', 'failure'] as $ratio) {
    [$numer, $denom] = explode('/', $ratio === 'failure' ? '125/3' : $ratio);
    foreach ($ratio === '125/3' || $ratio === 'failure' ? ['0', '1'] : ['0'] as $selfcounts) {
        echo runCpuTimeCommand([PHP_BINARY, '-n', '-d', 'extension=' . $module, __DIR__ . '/cpu-time-shim.inc'], [
            'PERFIDIOUS_TEST_RATIO' => $ratio,
            'PERFIDIOUS_TEST_NUMER' => $numer,
            'PERFIDIOUS_TEST_DENOM' => $denom,
            'PERFIDIOUS_TEST_TIMEBASE_FAILURE' => $ratio === 'failure' ? '1' : '0',
            'PERFIDIOUS_TEST_SELFCOUNTS' => $selfcounts,
        ]);
    }
}
?>
--EXPECT--
Darwin CPU-time conversion passed: 125/3
Darwin CPU-time conversion passed: 125/3
Darwin CPU-time conversion passed: 1/1
Darwin CPU-time conversion passed: 2/3
Darwin CPU-time conversion passed: 3/2
Darwin CPU-time conversion passed: 0/3
Darwin CPU-time conversion passed: 125/0
Darwin CPU-time conversion passed: failure
Darwin CPU-time conversion passed: failure
