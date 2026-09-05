--TEST--
Counter descriptors close on exec while raw streams retain independent ownership
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-linux-only.inc';
if (!is_dir('/proc/self/fdinfo')) die('skip: /proc/self/fdinfo is unavailable');
if (!function_exists('proc_open') || !function_exists('exec')) die('skip: process execution is unavailable');
?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
--FILE--
<?php

function perfDescriptors(): array
{
    $descriptors = [];
    foreach (scandir('/proc/self/fd') as $fd) {
        // The directory descriptor used by scandir() has already closed.
        if (@readlink('/proc/self/fd/' . $fd) !== 'anon_inode:[perf_event]') {
            continue;
        }
        $info = file_get_contents('/proc/self/fdinfo/' . $fd);
        if (!preg_match('/^flags:\s+([0-7]+)$/m', $info, $matches)) {
            throw new RuntimeException('Missing descriptor flags');
        }
        $descriptors[$fd] = (octdec($matches[1]) & 02000000) !== 0;
    }
    return $descriptors;
}

function reportFlags(string $label, array $descriptors): void
{
    printf("%s: %d descriptors, %d close-on-exec\n", $label, count($descriptors), count(array_filter($descriptors)));
}

$request = Perfidious\request_handle();
$requestDescriptors = perfDescriptors();
reportFlags('request', $requestDescriptors);
$handle = Perfidious\open(['perf::PERF_COUNT_SW_CPU_CLOCK:u', 'perf::PERF_COUNT_SW_PAGE_FAULTS:u']);
$ownedDescriptors = perfDescriptors();
reportFlags('owned', array_diff_key($ownedDescriptors, $requestDescriptors));
$streams = [$request->rawStream(), $request->rawStream(1)];
foreach ([0, 1, 2] as $idx) {
    $streams[] = $handle->rawStream($idx);
}
$allDescriptors = perfDescriptors();
reportFlags('raw streams', array_diff_key($allDescriptors, $ownedDescriptors));
if (in_array(false, $allDescriptors, true)) {
    // Check flags before exercising the process boundary.
    exit("Missing close-on-exec flags\n");
}

$child = <<<'PHP'
$count = 0;
foreach (scandir('/proc/self/fd') as $fd) {
    $count += @readlink('/proc/self/fd/' . $fd) === 'anon_inode:[perf_event]';
}
echo "child perf descriptors: $count\n";
PHP;
$command = [PHP_BINARY, '-n', '-d', 'perfidious.request.enable=0', '-r', $child];
$shellCommand = implode(' ', array_map('escapeshellarg', $command));
foreach (['direct' => $command, 'shell' => $shellCommand] as $kind => $invocation) {
    $process = proc_open($invocation, [['pipe', 'r'], ['pipe', 'w'], ['redirect', 1]], $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException('Could not start child');
    }
    fclose($pipes[0]);
    $output = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    printf("proc_open %s: %s", $kind, $output);
    printf("exit: %d\n", proc_close($process));
}
exec($shellCommand, $outputLines, $status);
printf("exec: %s\nexit: %d\n", implode("\n", $outputLines), $status);

// Launching children must not close descriptors in the parent.
var_dump(perfDescriptors() === $allDescriptors);
$handle->enable();
var_dump(count($handle->readArray()) === 2);
// Closing one duplicate must not close its original or the other duplicates.
fclose($streams[4]);
var_dump(count($handle->readArray()) === 2);
$handle->close();
printf("raw stream after handle close: %d bytes\n", strlen(fread($streams[2], 32)));
foreach (array_slice($streams, 0, 4) as $stream) {
    fclose($stream);
}
var_dump(perfDescriptors() === $requestDescriptors);
?>
--EXPECT--
request: 2 descriptors, 2 close-on-exec
owned: 3 descriptors, 3 close-on-exec
raw streams: 5 descriptors, 5 close-on-exec
proc_open direct: child perf descriptors: 0
exit: 0
proc_open shell: child perf descriptors: 0
exit: 0
exec: child perf descriptors: 0
exit: 0
bool(true)
bool(true)
bool(true)
raw stream after handle close: 32 bytes
bool(true)
