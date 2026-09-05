<?php
$opens = Perfidious\DEBUG ? Perfidious\debug_get_open_ex_call_count() : null;
$error = null;
try {
    Perfidious\request_handle();
} catch (Throwable $caught) {
    $error = [
        'class' => get_class($caught),
        'code' => $caught->getCode(),
        'message' => $caught->getMessage(),
    ];
}

header('Content-Type: application/json');
echo json_encode([
    'pid' => getmypid(),
    'opens' => $opens,
    'error' => $error,
    'consumed' => Perfidious\request_handle() === null,
]), "\n";
