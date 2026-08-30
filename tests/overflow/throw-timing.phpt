--TEST--
overflow in Handle timing fields throws
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
<?php if (!is_dir('/proc/self/fd')) die('skip: /proc/self/fd is unavailable'); ?>
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
function injectTimingRead(Perfidious\Handle $handle, string $timeEnabled, string $timeRunning)
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

$handle = Perfidious\open([]);
$uint64Max = str_repeat("\xff", 8);
$one = pack('Q', 1);

foreach (
    [
        'timeEnabled' => [$uint64Max, $one],
        'timeRunning' => [$one, $uint64Max],
    ] as $field => [$timeEnabled, $timeRunning]
) {
    $stream = injectTimingRead($handle, $timeEnabled, $timeRunning);

    try {
        $handle->read();
        echo "$field unexpectedly succeeded\n";
    } catch (Perfidious\OverflowException $e) {
        echo "$field: ", $e->getMessage(), "\n";
    } finally {
        fclose($stream);
    }
}
--EXPECTF--
timeEnabled: value too large: %d > %d
timeRunning: value too large: %d > %d
