--TEST--
Windows thread profiling conflicts throw Perfidious\ResourceBusyException
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-windows-only.inc'; ?>
--FILE--
<?php

use Perfidious\Metric;
use Perfidious\ResourceBusyException;
use Perfidious\Sampler;
use Perfidious\Scope;

function expectBusy(string $operation, int $attempt, Closure $enable): void
{
    try {
        $unexpected = $enable();
        $unexpected->close();
        echo "$operation attempt $attempt unexpectedly succeeded\n";
    } catch (ResourceBusyException $exception) {
        printf(
            "%s attempt %d: code %d, native I/O: %s\n",
            $operation,
            $attempt,
            $exception->getCode(),
            $exception instanceof Perfidious\IOException ? 'yes' : 'no',
        );
    }
}

$sampler = Sampler::open([Metric::ContextSwitches], Scope::CurrentThread);
$samplerValue = $sampler->read()->value(Metric::ContextSwitches);
for ($attempt = 1; $attempt <= 2; $attempt++) {
    expectBusy(
        'low-level profile while sampler is active',
        $attempt,
        static fn() => Perfidious\Windows\enable_current_thread_profiling(),
    );
    $nextSamplerValue = $sampler->read()->value(Metric::ContextSwitches);
    printf(
        "sampler owner usable after attempt %d: %s\n",
        $attempt,
        $nextSamplerValue >= $samplerValue ? 'yes' : 'no',
    );
    $samplerValue = $nextSamplerValue;
}
$sampler->close();
$sampler->close();

$profile = Perfidious\Windows\enable_current_thread_profiling();
$profileSnapshot = $profile->read();
for ($attempt = 1; $attempt <= 2; $attempt++) {
    expectBusy(
        'sampler while low-level profile is active',
        $attempt,
        static fn() => Sampler::open([Metric::ContextSwitches], Scope::CurrentThread),
    );
    $nextProfileSnapshot = $profile->read();
    printf(
        "low-level owner usable after attempt %d: %s\n",
        $attempt,
        $nextProfileSnapshot->contextSwitchCount >= $profileSnapshot->contextSwitchCount &&
            $nextProfileSnapshot->cycleCount >= $profileSnapshot->cycleCount
            ? 'yes'
            : 'no',
    );
    $profileSnapshot = $nextProfileSnapshot;
}
$profile->close();
$profile->close();

$samplerAfterRelease = Sampler::open([Metric::ContextSwitches], Scope::CurrentThread);
var_dump(is_int($samplerAfterRelease->read()->value(Metric::ContextSwitches)));
$samplerAfterRelease->close();
$profileAfterRelease = Perfidious\Windows\enable_current_thread_profiling();
var_dump($profileAfterRelease->read() instanceof Perfidious\Windows\ThreadProfileSnapshot);
$profileAfterRelease->close();
--EXPECT--
low-level profile while sampler is active attempt 1: code 4206, native I/O: no
sampler owner usable after attempt 1: yes
low-level profile while sampler is active attempt 2: code 4206, native I/O: no
sampler owner usable after attempt 2: yes
sampler while low-level profile is active attempt 1: code 4206, native I/O: no
low-level owner usable after attempt 1: yes
sampler while low-level profile is active attempt 2: code 4206, native I/O: no
low-level owner usable after attempt 2: yes
bool(true)
bool(true)
