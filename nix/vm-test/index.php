<?php

$pid = getmypid();

function respond(array $body, int $status = 200): never
{
    http_response_code($status);
    echo json_encode($body);
    exit;
}

if (isset($_GET["failRequestHandleShutdown"])) {
    if (!\Perfidious\DEBUG) {
        respond(["error" => "debug support is disabled"], 500);
    }

    \Perfidious\debug_fail_next_request_handle_shutdown();
    respond(["pid" => $pid, "requestHandleShutdownFailureInjected" => true]);
}

if (isset($_GET["breakRequestHandle"])) {
    if (!\Perfidious\DEBUG) {
        respond(["error" => "debug support is disabled"], 500);
    }

    $requestHandle = \Perfidious\request_handle();
    if ($requestHandle === null) {
        respond(["error" => "request handle not enabled"], 500);
    }

    $requestHandle->debugCloseFd();
    respond(["pid" => $pid, "requestHandleBroken" => true]);
}

$g = \Perfidious\global_handle();
try {
    $r = \Perfidious\request_handle();
} catch (\Perfidious\IOException $error) {
    respond(["pid" => $pid, "requestHandleError" => $error->getCode()]);
}

if ($g === null || $r === null) {
    respond(["error" => "global/request handle not enabled"], 500);
}

foreach (["global" => $g, "request" => $r] as $name => $h) {
    foreach ($h->readArray() as $metric => $v) {
        if (!is_int($v) || $v < 0) {
            respond(["error" => "corrupt $name counter $metric: " . var_export($v, true)], 500);
        }
    }
}

respond(["pid" => $pid]);
