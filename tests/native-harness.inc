<?php

function perfidious_test_find_native_tool(string $name): ?string
{
    $testPhp = getenv('TEST_PHP_EXECUTABLE');
    $directories = explode(PATH_SEPARATOR, (string) getenv('PATH'));
    array_unshift($directories, dirname(is_string($testPhp) && $testPhp !== '' ? $testPhp : PHP_BINARY));
    foreach ($directories as $directory) {
        $candidate = rtrim($directory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name;
        if (is_executable($candidate)) {
            return $candidate;
        }
    }

    return null;
}

function perfidious_test_native_skipif(): void
{
    if (!function_exists('proc_open')) {
        die('skip: process execution is unavailable');
    }
    foreach (['cc', 'php-config'] as $name) {
        if (perfidious_test_find_native_tool($name) === null) {
            die('skip: ' . $name . ' is unavailable');
        }
    }
}

function perfidious_test_run_native_command(array $command): array
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

function perfidious_test_run_native_harness(string $source, string $label, array $flags = []): string
{
    $executables = [];
    foreach (['cc', 'php-config'] as $name) {
        $executables[$name] = perfidious_test_find_native_tool($name)
            ?? throw new RuntimeException($name . ' disappeared after SKIPIF');
    }

    [$includeStatus, $includeFlags] = perfidious_test_run_native_command([$executables['php-config'], '--includes']);
    if ($includeStatus !== 0) {
        throw new RuntimeException('php-config failed: ' . $includeFlags);
    }
    $binary = sprintf('%s/perfidious-native-harness-%d-%s', sys_get_temp_dir(), getmypid(), bin2hex(random_bytes(8)));
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
        '-I' . dirname(__DIR__),
        ...$flags,
        $source,
        '-o',
        $binary,
    ];
    [$compileStatus, $compileOutput] = perfidious_test_run_native_command($command);
    if ($compileStatus !== 0) {
        throw new RuntimeException($label . ' did not compile: ' . $compileOutput);
    }
    [$status, $output] = perfidious_test_run_native_command([$binary]);
    if ($status !== 0) {
        throw new RuntimeException($label . ' failed: ' . $output);
    }
    return $output;
}
