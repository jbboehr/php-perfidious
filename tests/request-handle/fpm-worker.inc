<?php
$event = 'perf::PERF_COUNT_SW_TASK_CLOCK:u';
$opens = Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null;
$request = Perfidious\request_handle();
$start = $request->readArray()[$event];
$fresh = Perfidious\open([$event]);
$fresh->enable();
$before = [$request->readArray()[$event], $fresh->readArray()[$event]];
// Keep both workers occupied long enough for the concurrent request to be accepted.
usleep(100_000);
$deadline = hrtime(true) + 100_000_000;
while (hrtime(true) < $deadline) {
    hash('sha256', 'request attribution');
}
$after = [$request->readArray()[$event], $fresh->readArray()[$event]];
$fresh->close();
header('Content-Type: application/json');
echo json_encode([
    'pid' => getmypid(),
    'opens' => $opens,
    'start' => $start,
    'request' => $after[0] - $before[0],
    'fresh' => $after[1] - $before[1],
]), "\n";
