"""Check Linux release packaging using real ELF files and ZIP archives."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def main():
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="perfidious-linux-package-") as temporary:
        directory = Path(temporary)
        source = directory / "fixture.c"
        source.write_text('extern int puts(const char *); void fixture(void) { puts("fixture"); }\n')
        glibc = directory / "glibc.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-nostdlib", str(source), "-lc", "-o", str(glibc)], check=True)
        subprocess.run(["patchelf", "--set-rpath", "", str(glibc)], check=True)
        subprocess.run(["patchelf", "--remove-rpath", str(glibc)], check=True)
        source.write_text("void fixture(void) {}\n")
        musl = directory / "musl.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-nostdlib", str(source), "-o", str(musl)], check=True)
        subprocess.run(["patchelf", "--set-rpath", "", str(musl)], check=True)
        subprocess.run(["patchelf", "--remove-rpath", str(musl)], check=True)
        subprocess.run(["patchelf", "--add-needed", "libc.musl-x86_64.so.1", str(musl)], check=True)

        commands = directory / "bin"
        commands.mkdir()
        (commands / "php").write_text('#!/bin/sh\nprintf "%s\\n" "$PACKAGE_TEST_PHP"\n')
        (commands / "php").chmod(0o755)
        environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"])
        cases = [
            ("glibc", "glibc", glibc, None, True),
            ("musl", "musl", musl, None, True),
            ("wrong-libc", "musl", glibc, None, False),
            ("shared-libcap", "glibc", glibc, ["--add-needed", "libcap.so.2"], False),
            ("shared-libpfm", "glibc", glibc, ["--add-needed", "libpfm.so.4"], False),
            ("rpath", "glibc", glibc, ["--set-rpath", "/nix/store/fixture/lib"], False),
            ("new-glibc", "glibc", glibc, None, False),
            ("arm64", "glibc", glibc, None, False),
            ("32-bit", "glibc", glibc, None, False),
            ("store-reference", "glibc", glibc, None, False),
            ("zts", "glibc", glibc, None, False),
            ("debug", "glibc", glibc, None, False),
            ("exported-libcap", "glibc", glibc, None, False),
            ("missing-license", "glibc", glibc, None, False),
        ]
        for name, libc, fixture, patch, success in cases:
            case = directory / name
            (case / "modules").mkdir(parents=True)
            (case / "docs").mkdir()
            (case / "dependencies").mkdir()
            module = case / "modules/perfidious.so"
            shutil.copyfile(fixture, module)
            for filename in ["LICENSE.md", "docs/LICENSE_EXCEPTION.md"]:
                shutil.copyfile(root / filename, case / filename)
            payloads = {
                "dependencies/libcap.tar.xz": b"libcap source fixture",
                "dependencies/libpfm.tar.gz": b"libpfm source fixture",
                "dependencies/README.txt": b"Dependency versions and licenses\n",
            }
            for filename, contents in payloads.items():
                (case / filename).write_bytes(contents)
            if patch:
                subprocess.run(["patchelf", *patch, str(module)], check=True)
            if name == "new-glibc":
                data = module.read_bytes()
                assert b"GLIBC_2.2.5" in data
                module.write_bytes(data.replace(b"GLIBC_2.2.5", b"GLIBC_9.9.9"))
            elif name == "arm64":
                data = bytearray(module.read_bytes())
                data[18:20] = (183).to_bytes(2, "little")
                module.write_bytes(data)
            elif name == "32-bit":
                data = bytearray(module.read_bytes())
                data[4] = 1
                module.write_bytes(data)
            elif name == "store-reference":
                with module.open("ab") as stream:
                    stream.write(b"/nix/store/fixture/lib/libcap.so\0")
            elif name == "missing-license":
                (case / "docs/LICENSE_EXCEPTION.md").unlink()
            elif name == "exported-libcap":
                source.write_text('extern int puts(const char *); void cap_fixture(void) { puts("fixture"); }\n')
                subprocess.run(["cc", "-shared", "-fPIC", "-nostdlib", str(source), "-lc", "-o", str(module)], check=True)
                subprocess.run(["patchelf", "--set-rpath", "", str(module)], check=True)
                subprocess.run(["patchelf", "--remove-rpath", str(module)], check=True)
            environment["PACKAGE_TEST_PHP"] = (
                '["Linux",8,"8.1",' + ("true" if name == "zts" else "false") + ','
                + ("true" if name == "debug" else "false") + ']'
            )
            output = case / "packages with spaces"
            result = subprocess.run(
                [sys.executable, str(root / "tools/package-linux.py"), "v0.3.0", libc, str(output)],
                cwd=case, env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                timeout=30,
            )
            assert (result.returncode == 0) == success, f"{name}: expected success={success}\n{result.stdout}"
            if success:
                archive = output / f"php_perfidious-v0.3.0_php8.1-x86_64-linux-{libc}-nts.zip"
                with zipfile.ZipFile(archive) as package:
                    expected = dict(payloads, **{
                        "perfidious.so": module.read_bytes(),
                        "LICENSE.md": (root / "LICENSE.md").read_bytes(),
                        "LICENSE_EXCEPTION.md": (root / "docs/LICENSE_EXCEPTION.md").read_bytes(),
                    })
                    assert set(package.namelist()) == set(expected)
                    for filename, contents in expected.items():
                        assert package.read(filename) == contents, filename
                        assert (package.getinfo(filename).external_attr >> 16) & 0o777 == 0o644, filename
                if name == "glibc":
                    original = archive.read_bytes()
                    os.utime(module, (1_700_000_000, 1_700_000_000))
                    subprocess.run(
                        [sys.executable, str(root / "tools/package-linux.py"), "v0.3.0", libc, str(output)],
                        cwd=case, env=environment, check=True, stdout=subprocess.PIPE,
                    )
                    assert archive.read_bytes() == original, "Package must not depend on staging timestamps"
            else:
                assert not list(output.glob("*.zip")), name
            print(f"{name}: passed", flush=True)


if __name__ == "__main__":
    main()
