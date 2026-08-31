--TEST--
Sampler reads Windows current-thread context switches and CPU cycles
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;

$metrics = [Metric::CpuTime, Metric::ContextSwitches, Metric::CpuCycles];
$sampler = Sampler::open($metrics, Scope::CurrentThread);
$before = $sampler->read();
$accumulator = 0;

for ($attempt = 0; $attempt < 4; $attempt++) {
    $deadline = hrtime(true) + 50_000_000;
    while (hrtime(true) < $deadline) {
        $accumulator = ($accumulator * 1664525 + 1013904223) & 0x7fffffff;
    }

    // Dispatch profiling is updated as the thread is switched, so yield after doing CPU work.
    for ($sleep = 0; $sleep < 8; $sleep++) {
        usleep(1_000);
    }

    $after = $sampler->read();
    $contextSwitchDelta =
        $after->value(Metric::ContextSwitches) - $before->value(Metric::ContextSwitches);
    $cycleDelta = $after->value(Metric::CpuCycles) - $before->value(Metric::CpuCycles);

    if ($contextSwitchDelta > 0 && $cycleDelta > 0) {
        break;
    }
}

$delta = $after->since($before);
$allValuesAreInts = true;
foreach ($metrics as $metric) {
    $allValuesAreInts = $allValuesAreInts &&
        is_int($before->value($metric)) &&
        is_int($after->value($metric)) &&
        is_int($delta->value($metric));
}

var_dump($sampler->metrics() === $metrics);
var_dump($allValuesAreInts);
var_dump(
    $contextSwitchDelta > 0 &&
    $delta->value(Metric::ContextSwitches) === $contextSwitchDelta
);
var_dump(
    $cycleDelta > 0 &&
    $delta->value(Metric::CpuCycles) === $cycleDelta
);

try {
    $unexpectedProfile = Perfidious\Windows\enable_current_thread_profiling();
    $unexpectedProfile->close();
} catch (Perfidious\ResourceBusyException) {
    echo "low-level conflict rejected\n";
}
$afterLowLevelConflict = $sampler->read();
var_dump(
    $afterLowLevelConflict->value(Metric::ContextSwitches) >= $after->value(Metric::ContextSwitches) &&
    $afterLowLevelConflict->value(Metric::CpuCycles) >= $after->value(Metric::CpuCycles)
);

$sampler->close();
$profileAfterClose = Perfidious\Windows\enable_current_thread_profiling();
$cpuTimeOnlySampler = Sampler::open([Metric::CpuTime], Scope::CurrentThread);
var_dump(is_int($cpuTimeOnlySampler->read()->value(Metric::CpuTime)));
$cpuTimeOnlySampler->close();
$beforeSamplerConflict = $profileAfterClose->read();
try {
    $unexpectedSampler = Sampler::open(
        [Metric::ContextSwitches, Metric::CpuCycles],
        Scope::CurrentThread,
    );
    $unexpectedSampler->close();
} catch (Perfidious\ResourceBusyException) {
    echo "sampler conflict rejected\n";
}
$afterSamplerConflict = $profileAfterClose->read();
var_dump(
    $afterSamplerConflict->contextSwitchCount >= $beforeSamplerConflict->contextSwitchCount &&
    $afterSamplerConflict->cycleCount >= $beforeSamplerConflict->cycleCount
);
$profileAfterClose->close();
echo "released after close\n";

$samplerDestroyedWhileOpen = Sampler::open([Metric::ContextSwitches], Scope::CurrentThread);
unset($samplerDestroyedWhileOpen);
gc_collect_cycles();
$profileAfterDestruction = Perfidious\Windows\enable_current_thread_profiling();
$profileAfterDestruction->close();
echo "released after destruction\n";

$fiberSampler = Sampler::open($metrics, Scope::CurrentThread);
$beforeFiber = $fiberSampler->read();
$reader = new Fiber(static fn() => $fiberSampler->read());
$reader->start();
$fromFiber = $reader->getReturn();
var_dump(
    $fromFiber->value(Metric::CpuTime) >= $beforeFiber->value(Metric::CpuTime) &&
    $fromFiber->value(Metric::ContextSwitches) >= $beforeFiber->value(Metric::ContextSwitches) &&
    $fromFiber->value(Metric::CpuCycles) >= $beforeFiber->value(Metric::CpuCycles)
);

$closer = new Fiber(static function () use ($fiberSampler): void {
    $fiberSampler->close();
    $fiberSampler->close();
});
$closer->start();
try {
    $fiberSampler->read();
    echo "sampler remained readable after fiber close\n";
} catch (Perfidious\ClosedException) {
    echo "fiber close released the sampler\n";
}

$profileAfterFiberClose = Perfidious\Windows\enable_current_thread_profiling();
$profileAfterFiberClose->close();
echo "released after fiber close\n";

var_dump(is_int($accumulator));
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
low-level conflict rejected
bool(true)
bool(true)
sampler conflict rejected
bool(true)
released after close
released after destruction
bool(true)
fiber close released the sampler
released after fiber close
bool(true)
