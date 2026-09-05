<?php
$result = [
    'pid' => getmypid(),
    'opens' => Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null,
    'metrics' => ini_get('perfidious.request.metrics'),
];
if (!isset($_GET['ignore'])) {
    $result['error'] = null;
    try {
        Perfidious\request_handle();
    } catch (Throwable $error) {
        $result['error'] = [get_class($error), $error->getCode()];
    }
    $handle = Perfidious\request_handle();
    $values = $handle?->readArray();
    $value = $values['perf::PERF_COUNT_SW_TASK_CLOCK:u'] ?? null;
    $result['usable'] = is_int($value) && $value >= 0;
}
header('Content-Type: application/json');
echo json_encode($result), "\n";
