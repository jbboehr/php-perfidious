--TEST--
Request metric lists share the event limit and defer oversized-list errors
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
require __DIR__ . '/fpm-worker-artifacts.inc';
if (!function_exists('proc_open')) die('skip: proc_open is unavailable');
if (perfidious_test_module_path() === null) die('skip: the loaded shared module is unavailable');
?>
--FILE--
<?php
require __DIR__ . '/fpm-worker-artifacts.inc';

$child = <<<'PHP'
echo "body started\n";
try {
    Perfidious\request_handle();
    echo "no error\n";
} catch (Throwable $error) {
    echo get_class($error), "\n";
}
var_dump(Perfidious\request_handle() === null);
if ($argv[1] === 'no-open' && Perfidious\DEBUG && Perfidious\debug_get_open_ex_call_count() !== 0) {
    throw new RuntimeException('Rejected or disabled metrics reached native opening');
}
PHP;

// An invalid first name avoids acquiring a large group at the accepted boundary.
$atLimit = implode(',', array_fill(0, 1000, 'missing-r11-event'));
$cases = [
    'empty' => ['', true, false],
    '1000 names' => [$atLimit, true, false],
    '1001 names' => [$atLimit . ',missing-r11-event', true, true],
    '1002 names' => [$atLimit . ',missing-r11-event,missing-r11-event', true, true],
    '1000 fields with trailing empty' => [implode(',', array_fill(0, 999, 'missing-r11-event')) . ',', true, false],
    '1001 fields with trailing empty' => [$atLimit . ',', true, true],
    '1000 empty fields' => [str_repeat(',', 999), true, false],
    '1001 empty fields' => [str_repeat(',', 1000), true, true],
    'disabled oversized list' => [$atLimit . ',missing-r11-event', false, true],
];

foreach ($cases as $label => [$metrics, $enabled, $noOpen]) {
    $process = proc_open([
        PHP_BINARY, '-n', '-d', 'extension=' . perfidious_test_module_path(),
        '-d', 'perfidious.request.enable=' . (int) $enabled,
        '-d', 'perfidious.request.metrics=' . $metrics,
        '-r', $child, '--', $noOpen ? 'no-open' : 'open',
    ], [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start request metric test');
    }
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    $status = proc_close($process);
    echo $label, ":\n", $output;
    if ($status !== 0) {
        throw new RuntimeException('Request metric child failed: ' . $status);
    }
}
?>
--EXPECT--
empty:
body started
Perfidious\PmuEventNotFoundException
bool(true)
1000 names:
body started
Perfidious\PmuEventNotFoundException
bool(true)
1001 names:
body started
Perfidious\OverflowException
bool(true)
1002 names:
body started
Perfidious\OverflowException
bool(true)
1000 fields with trailing empty:
body started
Perfidious\PmuEventNotFoundException
bool(true)
1001 fields with trailing empty:
body started
Perfidious\OverflowException
bool(true)
1000 empty fields:
body started
Perfidious\PmuEventNotFoundException
bool(true)
1001 empty fields:
body started
Perfidious\OverflowException
bool(true)
disabled oversized list:
body started
no error
bool(true)
