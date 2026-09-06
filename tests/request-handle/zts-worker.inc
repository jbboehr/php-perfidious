<?php

function ztsWorkerCheck(bool $condition, string $message): void
{
    if (!$condition) {
        [$worker, $request] = fixture_zts_context();
        throw new RuntimeException("Worker $worker request $request: $message");
    }
}

[$worker, $request] = fixture_zts_context();
$event = 'perf::PERF_COUNT_SW_TASK_CLOCK:u';
$handle = Perfidious\request_handle();
ztsWorkerCheck($handle instanceof Perfidious\Handle, 'missing request handle');
$groupStream = $handle->rawStream();
$eventStream = $handle->rawStream(1);
$groupId = fixture_zts_event_id($groupStream);
$eventId = fixture_zts_event_id($eventStream);
fclose($groupStream);
fclose($eventStream);
fixture_zts_observe($groupId, $eventId);

// Only one worker burns CPU in each phase. Swapped ownership must not pass merely
// because both threads eventually performed the same amount of work.
$turns = $request < 2 ? [0, 1] : [$worker];
foreach ($turns as $turn) {
    $before = $handle->readArray()[$event];
    $cpuBefore = fixture_zts_cpu_time();
    if ($request < 2) {
        fixture_zts_barrier();
    }
    if ($worker === $turn) {
        fixture_zts_work();
    }
    if ($request < 2) {
        fixture_zts_barrier();
    }
    $cpuDelta = fixture_zts_cpu_time() - $cpuBefore;
    $delta = $handle->readArray()[$event] - $before;
    ztsWorkerCheck($delta >= 0, 'counter moved backwards');
    if ($worker === $turn) {
        ztsWorkerCheck($cpuDelta >= 40_000_000, 'workload did not consume thread CPU time');
        ztsWorkerCheck(
            $delta >= $cpuDelta / 2 && $delta <= $cpuDelta * 2,
            "request counter does not measure its worker: counter=$delta cpu=$cpuDelta",
        );
    } else {
        ztsWorkerCheck(
            $delta <= $cpuDelta * 2 + 2_000_000,
            "idle worker counted its peer's work: counter=$delta cpu=$cpuDelta",
        );
    }
}
