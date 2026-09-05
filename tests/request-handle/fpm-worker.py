"""Exercise request-counter lifecycle behavior in real FPM workers."""

import concurrent.futures
import grp
import json
import os
from pathlib import Path
import pwd
import socket
import struct
import subprocess
import sys
import tempfile
import time


def check(condition, details):
    if not condition:
        raise RuntimeError(f"FPM lifecycle check failed: {details!r}")


def record(kind, data=b""):
    return struct.pack("!BBHHBB", 1, kind, 1, len(data), 0, 0) + data


def length(value):
    return bytes([value]) if value < 128 else struct.pack("!I", value | 0x80000000)


def receive(connection, count):
    result = b""
    while len(result) < count:
        chunk = connection.recv(count - len(result))
        if not chunk:
            raise RuntimeError("Unexpected end of FastCGI response")
        result += chunk
    return result


def request(socket_path, script, metrics=None, query=""):
    values = {
        "GATEWAY_INTERFACE": "CGI/1.1",
        "REQUEST_METHOD": "GET",
        "SCRIPT_FILENAME": str(script),
        "SCRIPT_NAME": "/worker.php",
        "REQUEST_URI": "/worker.php",
        "SERVER_PROTOCOL": "HTTP/1.1",
        "SERVER_NAME": "localhost",
        "SERVER_PORT": "80",
        "REMOTE_ADDR": "127.0.0.1",
        "QUERY_STRING": query,
    }
    if metrics is not None:
        values["PHP_ADMIN_VALUE"] = "perfidious.request.metrics=" + metrics
    params = b""
    for name, value in values.items():
        name, value = name.encode(), value.encode()
        params += length(len(name)) + length(len(value)) + name + value
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(15)
        connection.connect(str(socket_path))
        connection.sendall(
            record(1, struct.pack("!HB5x", 1, 0)) + record(4, params) + record(4) + record(5)
        )
        output, errors = b"", b""
        while True:
            _, kind, _, count, padding, _ = struct.unpack("!BBHHBB", receive(connection, 8))
            data = receive(connection, count)
            receive(connection, padding)
            if kind == 6:
                output += data
            elif kind == 7:
                errors += data
            elif kind == 3:
                break
        if errors:
            raise RuntimeError(errors.decode())
        return json.loads(output.split(b"\r\n\r\n", 1)[1])


def main():
    fpm, module, opcache, mode = sys.argv[1:]
    if mode not in {"worker", "initialization-error", "unconsumed-error", "preload-disabled"}:
        raise RuntimeError(f"Unknown FPM test mode: {mode}")
    # Non-root preloading runs in the master itself, which exercises inherited handles.
    if opcache and os.geteuid() == 0:
        raise RuntimeError("Run the preload test as a non-root user")
    with tempfile.TemporaryDirectory(prefix="perfidious-fpm-") as directory:
        root = Path(directory)
        script = root / "worker.php"
        fixture = Path(__file__).with_name(
            "fpm-worker.inc" if mode == "worker" else f"fpm-{mode}.inc"
        )
        script.write_bytes(fixture.read_bytes())
        config = root / "fpm.conf"
        config.write_text(f"""[global]
daemonize = no
error_log = {root / 'fpm.log'}
[test]
user = {pwd.getpwuid(os.getuid()).pw_name}
group = {grp.getgrgid(os.getgid()).gr_name}
listen = {root / 'fpm.sock'}
pm = static
pm.max_children = {2 if mode == 'worker' else 1}
catch_workers_output = yes
clear_env = no
security.limit_extensions = .php
""")
        if mode == "preload-disabled":
            with config.open("a") as stream:
                stream.write("php_admin_value[perfidious.request.enable] = 0\n")
        command = [
            fpm, "-F", "-R", "-n", "-y", str(config),
            "-d", f"extension={module}",
            "-d", "perfidious.request.enable=1",
            "-d", "perfidious.request.metrics=" + (
                "blahblahblah" if mode == "initialization-error"
                else "perf::PERF_COUNT_SW_TASK_CLOCK:u"
            ),
        ]
        if opcache:
            preload = root / "preload.php"
            preload.write_text("""<?php
function perfidious_preloaded() {}
$fds = array_filter(glob('/proc/self/fd/*'), static function ($fd) {
    return str_contains((string) @readlink($fd), 'perf_event');
});
file_put_contents(__DIR__ . '/preload.json', json_encode(['pid' => getmypid(), 'perfFds' => count($fds)]));
""")
            command += ["-d", f"zend_extension={opcache}", "-d", f"opcache.preload={preload}"]
        with (root / "startup.log").open("w+") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
            try:
                for _ in range(200):
                    if process.poll() is not None:
                        log.seek(0)
                        raise RuntimeError(log.read())
                    if (root / "fpm.sock").exists():
                        break
                    time.sleep(0.025)
                else:
                    raise RuntimeError("FPM did not create its socket")
                if mode == "preload-disabled":
                    master = json.loads((root / "preload.json").read_text())
                    results = [request(root / "fpm.sock", script) for _ in range(2)]
                    print(json.dumps({"master": master, "results": results}))
                    check(master == {"pid": process.pid, "perfFds": 2}, master)
                    check(len({result["pid"] for result in results}) == 1, results)
                    for result in results:
                        check(result["pid"] != master["pid"], result)
                        check(result["enabled"] == "0" and result["handle"] is None, result)
                        check(result["perfFds"] == 0, result)
                        if result["opens"] is not None:
                            check(result["opens"] == 1, result)
                elif mode == "unconsumed-error":
                    ignored = request(root / "fpm.sock", script, "blahblahblah", "ignore=1")
                    recovered = request(root / "fpm.sock", script, "perf::PERF_COUNT_SW_TASK_CLOCK:u")
                    following = request(root / "fpm.sock", script)
                    print(json.dumps([ignored, recovered, following]))
                    check(ignored["pid"] == recovered["pid"] == following["pid"], (ignored, recovered, following))
                    check(ignored["metrics"] == "blahblahblah", ignored)
                    check(
                        recovered["metrics"] == following["metrics"] == "perf::PERF_COUNT_SW_TASK_CLOCK:u",
                        (recovered, following),
                    )
                    check(recovered["error"] == ["Perfidious\\PmuEventNotFoundException", -4], recovered)
                    check(following["error"] is None, following)
                    check(recovered["usable"] and following["usable"], (recovered, following))
                    if ignored["opens"] is not None:
                        check(
                            [ignored["opens"], recovered["opens"], following["opens"]] == [1, 2, 2],
                            (ignored, recovered, following),
                        )
                elif mode == "initialization-error":
                    results = [request(root / "fpm.sock", script) for _ in range(2)]
                    print(json.dumps({"master": process.pid, "results": results}))
                    check(len({result["pid"] for result in results}) == 1, results)
                    for number, result in enumerate(results, 1):
                        check(
                            result["error"] == {
                                "class": "Perfidious\\PmuEventNotFoundException",
                                "code": -4,
                                "message": (
                                    "failed to get libpfm event encoding for blahblahblah: "
                                    "event not found"
                                ),
                            },
                            result,
                        )
                        check(result["consumed"] is True, result)
                        if result["opens"] is not None:
                            check(result["opens"] == number, result)
                else:
                    rounds = []
                    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                        for _ in range(2):
                            jobs = [pool.submit(request, root / "fpm.sock", script) for _ in range(2)]
                            rounds.append([job.result() for job in jobs])
                    print(json.dumps({"master": process.pid, "rounds": rounds}))
                    pids = {result["pid"] for result in rounds[0]}
                    check(len(pids) == 2 and process.pid not in pids, rounds)
                    check({result["pid"] for result in rounds[1]} == pids, rounds)
                    for number, results in enumerate(rounds):
                        for result in results:
                            # A fresh counter is the oracle; never excuse a zero request counter.
                            check(result["fresh"] > 0, ("fresh task clock did not advance", result))
                            check(0.5 < result["request"] / result["fresh"] < 2, result)
                            check(result["start"] < result["fresh"] / 2, result)
                            if result["opens"] is not None:
                                # One automatic open per worker; each prior workload opens one oracle.
                                check(result["opens"] == 1 + bool(opcache) + number, result)
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
