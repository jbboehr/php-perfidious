"""Exercise macOS packaging with controlled lipo/otool output and real ZIP files."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile


def main():
    root = Path(__file__).resolve().parents[1]
    script = root / "nix/package-darwin.sh"
    php_version = subprocess.check_output(
        ["php", "-r", 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;'], text=True,
    )
    php_ts = subprocess.check_output(
        ["php", "-r", 'echo PHP_ZTS ? "zts" : "nts";'], text=True,
    )

    with tempfile.TemporaryDirectory(prefix="perfidious-darwin-package-") as temporary:
        directory = Path(temporary)
        commands = directory / "bin"
        commands.mkdir()
        (commands / "lipo").write_text(
            '#!/bin/sh\n[ "$1" = "-archs" ] || exit 1\n'
            'printf "%s\\n" "$PACKAGE_TEST_ARCH"\n'
        )
        (commands / "otool").write_text(
            '#!/bin/sh\n[ "$PACKAGE_TEST_OTOOL_FAIL" = "0" ] || exit 1\n'
            'case "$1" in\n'
            '  -L) printf "%s:\\n\\t%s (compatibility version 1.0.0, current version 1.0.0)\\n" '
            '"$2" "$PACKAGE_TEST_LIBRARY" ;;\n'
            '  -l) printf "%s:\\nLoad command 0\\n      cmd LC_BUILD_VERSION\\n'
            '  cmdsize 32\\n platform 1\\n    minos %s\\n      sdk 15.5\\n" '
            '"$2" "$PACKAGE_TEST_MINOS"; printf "%s\\n" "$PACKAGE_TEST_RPATH" ;;\n'
            '  *) exit 1 ;;\nesac\n'
        )
        for name in ["lipo", "otool"]:
            (commands / name).chmod(0o755)

        environment = dict(
            os.environ,
            PATH=str(commands) + os.pathsep + os.environ["PATH"],
            PACKAGE_TEST_ARCH="arm64",
            PACKAGE_TEST_LIBRARY="/usr/lib/libproc.dylib",
            PACKAGE_TEST_MINOS="11.0",
            PACKAGE_TEST_OTOOL_FAIL="0",
            PACKAGE_TEST_RPATH="",
        )
        cases = [
            ("system-library", {}, True),
            ("system-framework", {"PACKAGE_TEST_LIBRARY": "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"}, True),
            ("intel", {"PACKAGE_TEST_ARCH": "x86_64"}, False),
            ("universal", {"PACKAGE_TEST_ARCH": "x86_64 arm64"}, False),
            ("homebrew", {"PACKAGE_TEST_LIBRARY": "/opt/homebrew/opt/example/lib/libexample.dylib"}, False),
            ("rpath", {"PACKAGE_TEST_LIBRARY": "@rpath/libexample.dylib"}, False),
            ("runtime-search-path", {"PACKAGE_TEST_RPATH": "Load command 1\n      cmd LC_RPATH\n  cmdsize 48\n     path /nix/store/fixture/lib (offset 12)"}, False),
            ("newer-os", {"PACKAGE_TEST_MINOS": "15.0"}, False),
            ("missing-os", {"PACKAGE_TEST_MINOS": ""}, False),
            ("otool-failed", {"PACKAGE_TEST_OTOOL_FAIL": "1"}, False),
        ]
        for name, overrides, success in cases:
            case = directory / name
            (case / "modules").mkdir(parents=True)
            (case / "docs").mkdir()
            (case / "modules/perfidious.so").write_bytes(b"fixture module\0")
            for filename in ["LICENSE.md", "docs/LICENSE_EXCEPTION.md"]:
                shutil.copyfile(root / filename, case / filename)
            output = case / "packages with spaces"
            result = subprocess.run(
                ["bash", str(script), "v0.3.0", str(output)], cwd=case,
                env=dict(environment, **overrides), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
            )
            if (result.returncode == 0) != success:
                raise RuntimeError(f"{name}: expected success={success}\n{result.stdout}")
            if success:
                archive = output / f"php_perfidious-v0.3.0_php{php_version}-arm64-darwin-bsdlibc-{php_ts}.zip"
                with zipfile.ZipFile(archive) as package:
                    assert set(package.namelist()) == {"perfidious.so", "LICENSE.md", "LICENSE_EXCEPTION.md"}
                    assert all(entry.extra == b"" for entry in package.infolist()), "Host-specific ZIP metadata"
                    assert package.read("perfidious.so") == b"fixture module\0"
                    assert package.read("LICENSE.md") == (root / "LICENSE.md").read_bytes()
                    assert package.read("LICENSE_EXCEPTION.md") == (root / "docs/LICENSE_EXCEPTION.md").read_bytes()
            else:
                assert not list(output.glob("*.zip")), name
            print(f"{name}: passed", flush=True)


if __name__ == "__main__":
    main()
