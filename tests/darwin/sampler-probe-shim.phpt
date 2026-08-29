--TEST--
Darwin sampler capability probing handles availability, errors, and validation precedence
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    die('skip: Unix C compiler harness');
}
if (!function_exists('proc_open')) {
    die('skip: process execution is unavailable');
}

$findExecutable = static function (string $name): ?string {
    if ($name === 'php-config') {
        $testPhp = getenv('TEST_PHP_EXECUTABLE');
        $candidate = dirname(is_string($testPhp) && $testPhp !== '' ? $testPhp : PHP_BINARY) . '/php-config';
        if (is_executable($candidate)) {
            return $candidate;
        }
    }

    foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $directory) {
        $candidate = rtrim($directory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name;
        if (is_executable($candidate)) {
            return $candidate;
        }
    }

    return null;
};

if ($findExecutable('cc') === null) {
    die('skip: C compiler is unavailable');
}
if ($findExecutable('php-config') === null) {
    die('skip: php-config is unavailable');
}
?>
--FILE--
<?php

function runProbeCommand(array $command): array
{
    $environment = getenv();
    if (!is_array($environment)) {
        $environment = [];
    }
    unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);

    $process = proc_open(
        $command,
        [
            ['pipe', 'r'],
            ['pipe', 'w'],
            ['pipe', 'w'],
        ],
        $pipes,
        null,
        $environment,
    );
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start probe command');
    }

    fclose($pipes[0]);
    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);

    return [proc_close($process), $stdout, $stderr];
}

$findExecutable = static function (string $name): ?string {
    if ($name === 'php-config') {
        $testPhp = getenv('TEST_PHP_EXECUTABLE');
        $candidate = dirname(is_string($testPhp) && $testPhp !== '' ? $testPhp : PHP_BINARY) . '/php-config';
        if (is_executable($candidate)) {
            return $candidate;
        }
    }

    foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $directory) {
        $candidate = rtrim($directory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name;
        if (is_executable($candidate)) {
            return $candidate;
        }
    }

    return null;
};
$compiler = $findExecutable('cc') ?? throw new RuntimeException('C compiler disappeared after SKIPIF');
$phpConfig = $findExecutable('php-config') ?? throw new RuntimeException('php-config disappeared after SKIPIF');
$preloadMarker = 'perfidious-test-preload';
putenv('LD_PRELOAD=' . $preloadMarker);
putenv('DYLD_INSERT_LIBRARIES=' . $preloadMarker);
[$includeExitCode, $includeFlags, $includeErrors] = runProbeCommand([$phpConfig, '--includes']);
if ($includeExitCode !== 0) {
    throw new RuntimeException(sprintf('php-config failed: %s', $includeErrors));
}

$root = dirname(__DIR__, 2);
$binary = sprintf('%s/perfidious-darwin-sampler-probe-%d-%s', sys_get_temp_dir(), getmypid(), bin2hex(random_bytes(8)));
register_shutdown_function(static function () use ($binary): void {
    @unlink($binary);
});

$command = [
    $compiler,
    '-D_GNU_SOURCE',
    '-std=c11',
    '-Wall',
    '-Wextra',
    '-Werror',
    sprintf('-DPERFIDIOUS_TEST_PRELOAD_MARKER="%s"', $preloadMarker),
    ...preg_split('/\s+/', trim($includeFlags)),
    '-I' . $root,
    '-I' . __DIR__ . '/shim',
    __DIR__ . '/sampler-probe-harness.c',
    '-o',
    $binary,
];
[$compileExitCode, , $compileErrors] = runProbeCommand($command);
if ($compileExitCode !== 0) {
    throw new RuntimeException(sprintf('Darwin sampler probe harness did not compile: %s', $compileErrors));
}

[$probeExitCode, $probeOutput, $probeErrors] = runProbeCommand([$binary]);
if ($probeExitCode !== 0) {
    throw new RuntimeException(sprintf('Darwin sampler probe harness failed: %s', $probeErrors));
}

echo $probeOutput;
?>
--EXPECT--
Darwin sampler probe harness passed
