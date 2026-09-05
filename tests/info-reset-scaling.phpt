--TEST--
phpinfo scales request counters using the timing interval since the latest reset
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

$event = 'perf::PERF_COUNT_SW_CPU_CLOCK:u';
$handle = Perfidious\request_handle();
// Use two real intervals to distinguish the latest baseline from the first one.
foreach ([1, 2] as $interval) {
    $deadline = hrtime(true) + 5_000_000;
    while (hrtime(true) < $deadline) {
        hash('sha256', 'reset scaling');
    }
    $handle->disable();
    $before = $handle->read();
    $handle->reset();
    $after = $handle->read();
    var_dump($after->values[$event] === 0);
    // Public reads retain the kernel's lifetime timing fields.
    var_dump($after->timeEnabled === $before->timeEnabled && $after->timeRunning === $before->timeRunning);
    if ($interval === 1) {
        $handle->enable();
    }
}
if ($before->timeEnabled <= 0 || $before->timeRunning <= 0) {
    throw new RuntimeException('The real counter did not advance');
}

// The next interval has count 50, enabled time 100, and running time 50.
// Lifetime timing would instead suggest approximately 100% running.
$stream = injectScalingRead($handle, pack('Q', $before->timeEnabled + 100), pack('Q', $before->timeRunning + 50));
$data = stream_get_contents($stream);
// Keep the complete group layout and IDs; only replace the named event's count.
$data = substr_replace($data, pack('Q', 50), 40, 8);
rewind($stream);
if (fwrite($stream, $data) !== strlen($data) || !fflush($stream)) {
    throw new RuntimeException('Could not write the interval count');
}
rewind($stream);
ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();
foreach (explode("\n", $info) as $line) {
    if (str_starts_with($line, "$event =>")) {
        echo $line, "\n";
    }
}
fclose($stream);
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
perf::PERF_COUNT_SW_CPU_CLOCK:u => 50 => 100 => 50%
