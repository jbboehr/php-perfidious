--TEST--
Sampler construction owns native resources before reading and releases failed opens
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Linux') {
    die('skip: the isolated construction fixture is built on Linux');
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
$process = proc_open(
    [PHP_BINARY, '-n', '-r', 'echo extension_loaded("perfidious") ? "static" : "shared";'],
    [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]],
    $pipes,
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
    die('skip: the isolated fixture requires PHP without a built-in perfidious extension');
}
?>
--FILE--
<?php

function constructionCommand(array $command): string
{
    $environment = getenv();
    unset($environment['LD_PRELOAD'], $environment['DYLD_INSERT_LIBRARIES']);
    unset($environment['PERFIDIOUS_TEST_CONSTRUCTION_FAILURE']);
    $process = proc_open($command, [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes, null, $environment);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start construction test command');
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
$module = tempnam(sys_get_temp_dir(), 'perfidious-construction-');
register_shutdown_function(static function () use ($module): void {
    @unlink($module);
});
$includes = preg_split('/\s+/', trim(constructionCommand(['php-config', '--includes'])));
constructionCommand([
    'cc', '-D_GNU_SOURCE', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    '-shared', '-fPIC', ...$includes, '-I' . $root,
    __DIR__ . '/construction-backend.c', $root . '/src/exceptions.c', '-o', $module,
]);
echo constructionCommand([PHP_BINARY, '-n', '-d', 'extension=' . $module, '-r', <<<'PHP'
use Perfidious\Metric;
use Perfidious\Sampler;

function check(bool $condition, string $detail = ''): void {
    if (!$condition) {
        throw new RuntimeException('Construction contract failed: ' . $detail);
    }
}

$survivor = Sampler::open([Metric::CpuTime]);
check(fixture_live() === 1);

foreach (['open', 'read'] as $attempt => $stage) {
    putenv('PERFIDIOUS_TEST_CONSTRUCTION_FAILURE=' . $stage);
    try {
        Sampler::open([Metric::CpuTime]);
        throw new RuntimeException('Expected construction failure');
    } catch (Perfidious\IOException $exception) {
        check($exception->getMessage() === 'Native ' . $stage . ' failed', $exception->getMessage());
    }
    check(fixture_live() === 1, $stage . ' failure disturbed another sampler');
    putenv('PERFIDIOUS_TEST_CONSTRUCTION_FAILURE');
    check($survivor->read()->value(Metric::CpuTime) === $attempt + 1);
    echo "$stage failure released only its own resources\n";
}
$survivor->close();
check(fixture_live() === 0);
// A closed sampler must not satisfy the next open's empty-owner check.
unset($survivor);
$first = Sampler::open([Metric::CpuTime]);
$second = Sampler::open([Metric::CpuTime]);
check(fixture_live() === 2);
check($first->read()->value(Metric::CpuTime) === 1);
check($second->read()->value(Metric::CpuTime) === 1);
$first->close();
$first->close();
check(fixture_live() === 1);
unset($first, $second);
check(fixture_live() === 0);
echo "successful construction, close and destruction released resources\n";
PHP]);
?>
--EXPECT--
open failure released only its own resources
read failure released only its own resources
successful construction, close and destruction released resources
