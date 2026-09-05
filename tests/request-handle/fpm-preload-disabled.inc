<?php
$fds = array_filter(glob('/proc/self/fd/*'), static function ($fd) {
    return str_contains((string) @readlink($fd), 'perf_event');
});
header('Content-Type: application/json');
echo json_encode([
    'pid' => getmypid(),
    'enabled' => ini_get('perfidious.request.enable'),
    'handle' => Perfidious\request_handle(),
    'opens' => Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null,
    'perfFds' => count($fds),
]), "\n";
