<?php

use Perfidious\Darwin\ProcessResourceUsage;
use Perfidious\Metric;
use Perfidious\Sampler;

$failures = [];
function check(bool $condition, string $label): void
{
    global $failures;
    if (!$condition) {
        $failures[] = $label;
    }
}

function ticks(string $user, string $system): void
{
    putenv('PERFIDIOUS_TEST_USER_TICKS=' . $user);
    putenv('PERFIDIOUS_TEST_SYSTEM_TICKS=' . $system);
}

function expectException(callable $operation, string $class, string $message): void
{
    try {
        $operation();
        check(false, 'missing ' . $class . ': ' . $message);
    } catch (Throwable $error) {
        check($error instanceof $class, 'wrong exception: ' . get_class($error));
        check(str_contains($error->getMessage(), $message), 'wrong diagnostic: ' . $error->getMessage());
    }
}

$ratio = getenv('PERFIDIOUS_TEST_RATIO');
$invalid = in_array($ratio, ['0/3', '125/0', 'failure'], true);
if ($invalid) {
    expectException(Perfidious\Darwin\get_current_process_resource_usage(...), Perfidious\IOException::class, 'mach_timebase_info');
    expectException(static fn() => Sampler::open([Metric::CpuTime]), Perfidious\IOException::class, 'mach_timebase_info');
    // Unavailable CPU-time conversion must not disable unrelated metrics.
    $cycles = Sampler::open([Metric::CpuCycles]);
    check($cycles->read()->value(Metric::CpuCycles) === 0, 'cycle-only sampler remains available');
    $cycles->close();
} else {
    [$userNs, $systemNs] = match ($ratio) {
        '1/1' => [18_000_000, 6_000_000],
        '2/3' => [12_000_000, 4_000_000],
        '3/2' => [27_000_000, 9_000_000],
        default => [750_000_000, 250_000_000],
    };
    $usage = Perfidious\Darwin\get_current_process_resource_usage();
    check($usage instanceof ProcessResourceUsage, 'process result type');
    check($usage->userTimeNs === $userNs, 'process user time must be nanoseconds');
    check($usage->systemTimeNs === $systemNs, 'process system time must be nanoseconds');
    check($usage->cycleCount === 12 && $usage->instructionCount === 34, 'hardware counts must remain unscaled');

    ticks('0', '0');
    $sampler = Sampler::open([Metric::CpuTime]);
    ticks('18000000', '6000000');
    check($sampler->read()->value(Metric::CpuTime) === $userNs + $systemNs, 'sampler time must be nanoseconds');
    $sampler->close();

    if ($ratio === '125/3') {
        ticks('18446744073709551615', '0');
        expectException(Perfidious\Darwin\get_current_process_resource_usage(...), Perfidious\OverflowException::class, 'scale');
        expectException(static fn() => Sampler::open([Metric::CpuTime]), Perfidious\OverflowException::class, 'scale');
        ticks('0', '18446744073709551615');
        expectException(Perfidious\Darwin\get_current_process_resource_usage(...), Perfidious\OverflowException::class, 'scale');
        expectException(static fn() => Sampler::open([Metric::CpuTime]), Perfidious\OverflowException::class, 'scale');

        ticks('221360928884514620', '0');
        expectException(Perfidious\Darwin\get_current_process_resource_usage(...), Perfidious\OverflowException::class, 'PHP integer');
        ticks('300000000000000000', '300000000000000000');
        expectException(static fn() => Sampler::open([Metric::CpuTime]), Perfidious\OverflowException::class, 'total CPU time');
    } elseif ($ratio === '2/3') {
        // A direct ticks * numer would overflow even though the final result fits.
        ticks('0', '0');
        $sampler = Sampler::open([Metric::CpuTime]);
        ticks('12000000000000000000', '0');
        check(Perfidious\Darwin\get_current_process_resource_usage()->userTimeNs === 8_000_000_000_000_000_000, 'large scaled value');
        check($sampler->read()->value(Metric::CpuTime) === 8_000_000_000_000_000_000, 'large sampler value');
        $sampler->close();

        ticks('0', '0');
        $sampler = Sampler::open([Metric::CpuTime]);
        ticks('1', '1');
        $usage = Perfidious\Darwin\get_current_process_resource_usage();
        check($usage->userTimeNs === 0 && $usage->systemTimeNs === 0, 'each field rounds down');
        check($sampler->read()->value(Metric::CpuTime) === 0, 'sampler agrees with separately converted fields');
        $sampler->close();
    } elseif ($ratio === '3/2') {
        // The whole term fits exactly, but adding the fractional term overflows uint64.
        ticks('12297829382473034411', '0');
        expectException(Perfidious\Darwin\get_current_process_resource_usage(...), Perfidious\OverflowException::class, 'scale');
        expectException(static fn() => Sampler::open([Metric::CpuTime]), Perfidious\OverflowException::class, 'scale');
    }
}

$thread = Perfidious\Darwin\get_current_thread_resource_usage();
$threadExpected = getenv('PERFIDIOUS_TEST_SELFCOUNTS') === '1' && $ratio !== 'failure'
    ? $userNs + $systemNs : 6_000_800_000;
check($thread->userTimeNs + $thread->systemTimeNs === $threadExpected, 'thread CPU time retains its unit contract');
if ($failures !== []) {
    throw new RuntimeException(implode("\n", $failures));
}
echo "Darwin CPU-time conversion passed: $ratio\n";
