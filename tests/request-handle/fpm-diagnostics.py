"""Check fixture failure reporting using real FPM and synthetic shutdown diagnostics."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fpm, module = sys.argv[1:]
    runner = Path(__file__).with_name("fpm-worker.py")
    with tempfile.TemporaryDirectory(prefix="perfidious-fpm-diagnostics-") as directory:
        wrapper = Path(directory) / "php-fpm"
        # Forward termination to real FPM, then report a controlled result after it exits.
        wrapper.write_text(f"""#!{sys.executable}
from pathlib import Path
import os
import signal
import subprocess
import sys

child = subprocess.Popen([{fpm!r}, *sys.argv[1:]])
signal.signal(signal.SIGTERM, lambda *_: child.terminate())
status = child.wait()
if status:
    sys.exit(status)
case = os.environ['PERFIDIOUS_FPM_DIAGNOSTIC_CASE']
if case == 'asan':
    print('ERROR: AddressSanitizer: synthetic shutdown diagnostic', file=sys.stderr)
elif case == 'ubsan':
    config = Path(sys.argv[sys.argv.index('-y') + 1])
    with (config.parent / 'fpm.log').open('a') as log:
        log.write('runtime error: synthetic worker diagnostic\\n')
elif case == 'exit':
    sys.exit(23)
""")
        wrapper.chmod(0o700)
        for case, marker in [
            ("clean", None),
            ("asan", "ERROR: AddressSanitizer: synthetic shutdown diagnostic"),
            ("ubsan", "runtime error: synthetic worker diagnostic"),
            ("exit", "23"),
        ]:
            result = subprocess.run(
                [sys.executable, str(runner), str(wrapper), module, "", "initialization-error"],
                env=os.environ | {"PERFIDIOUS_FPM_DIAGNOSTIC_CASE": case},
                capture_output=True,
                text=True,
                timeout=30,
            )
            output = result.stdout + result.stderr
            if marker is None:
                if result.returncode != 0:
                    raise RuntimeError(f"Clean FPM run failed: {output}")
            elif result.returncode == 0 or marker not in output:
                raise RuntimeError(f"Fixture did not report {case}: status={result.returncode}; {output}")


if __name__ == "__main__":
    main()
