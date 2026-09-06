--TEST--
Request counters belong to their ZTS workers, survive repeated requests, and close on thread destruction
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
if (!PHP_ZTS || PHP_INT_SIZE !== 8) {
    die('skip: requires 64-bit ZTS PHP');
}
if (!function_exists('proc_open')) {
    die('skip: process execution is unavailable');
}
foreach (['cc', 'php-config', 'timeout'] as $tool) {
    $found = false;
    foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $directory) {
        $found = $found || is_executable($directory . '/' . $tool);
    }
    if (!$found) {
        die('skip: ' . $tool . ' is unavailable');
    }
}
require __DIR__ . '/fpm-worker-artifacts.inc';
if (perfidious_test_module_path() === null) {
    die('skip: requires a shared perfidious module');
}
?>
--FILE--
<?php
require __DIR__ . '/fpm-worker-artifacts.inc';

function ztsCommand(array $command): string
{
    $environment = getenv();
    unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);
    $process = proc_open($command, [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes, null, $environment);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start ZTS fixture command');
    }
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    $status = proc_close($process);
    if ($status !== 0) {
        throw new RuntimeException("ZTS fixture command exited $status:\n$output");
    }
    return $output;
}

$fixture = tempnam(sys_get_temp_dir(), 'perfidious-zts-');
register_shutdown_function(static function () use ($fixture): void {
    @unlink($fixture);
});
$includes = preg_split('/\s+/', trim(ztsCommand(['php-config', '--includes'])));
ztsCommand([
    'cc', '-D_GNU_SOURCE', '-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1', '-std=c11',
    '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-shared', '-fPIC', '-pthread',
    ...$includes, __DIR__ . '/zts-worker.c', '-o', $fixture,
]);
echo ztsCommand([
    'timeout', '--kill-after=5', '30', PHP_BINARY, '-n', '-d', 'extension=' . perfidious_test_module_path(),
    '-d', 'extension=' . $fixture, '-d', 'max_execution_time=0', '-d', 'perfidious.request.enable=1',
    '-d', 'perfidious.request.metrics=perf::PERF_COUNT_SW_TASK_CLOCK:u', '-r', <<<'PHP'
$main = Perfidious\request_handle();
$stream = $main->rawStream();
$id = fixture_zts_event_id($stream);
fclose($stream);
for ($wave = 0; $wave < 2; ++$wave) {
    fixture_zts_run($argv[1]);
    $stream = $main->rawStream();
    if (fixture_zts_event_id($stream) !== $id) {
        throw new RuntimeException('Worker teardown disturbed the main request');
    }
    fclose($stream);
    $before = $main->readArray()['perf::PERF_COUNT_SW_TASK_CLOCK:u'];
    fixture_zts_work();
    if ($main->readArray()['perf::PERF_COUNT_SW_TASK_CLOCK:u'] <= $before) {
        throw new RuntimeException('Main request counter stopped after worker teardown');
    }
}
echo "thread attribution, reuse, reset and teardown passed\n";
PHP,
    '--', __DIR__ . '/zts-worker.inc',
]);
?>
--EXPECT--
thread attribution, reuse, reset and teardown passed
