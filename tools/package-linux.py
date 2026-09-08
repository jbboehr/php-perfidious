"""Validate and package a Linux release module."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("libc", choices=["glibc", "musl"])
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    module = Path("modules/perfidious.so")

    os_family, integer_size, php_version, zts, debug = json.loads(subprocess.check_output([
        "php", "-n", "-r",
        'echo json_encode([PHP_OS_FAMILY, PHP_INT_SIZE, PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION, (bool) PHP_ZTS, (bool) PHP_DEBUG]);',
    ], text=True))
    if os_family != "Linux" or integer_size != 8 or zts or debug:
        raise RuntimeError("Linux release packages require a 64-bit NTS, non-debug PHP build")

    data = module.read_bytes()
    # ELF64, little endian, ET_DYN, EM_X86_64. Reject other targets before inspecting their dynamic sections.
    if data[:6] != b"\x7fELF\x02\x01" or data[16:20] != b"\x03\x00\x3e\x00":
        raise RuntimeError("Linux release packages require an x86-64 shared module")
    details = subprocess.check_output(
        ["readelf", "--wide", "--dynamic", "--version-info", "--dyn-syms", str(module)],
        text=True, env=dict(os.environ, LC_ALL="C"),
    )
    needed = set(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", details))
    expected = {"libc.so.6"} if args.libc == "glibc" else {"libc.musl-x86_64.so.1"}
    if needed != expected:
        raise RuntimeError(f"Unexpected shared dependencies for {args.libc}: {sorted(needed)}")
    if "(RPATH)" in details or "(RUNPATH)" in details or b"/nix/store/" in data:
        raise RuntimeError("Release modules must not contain runtime search paths or Nix store references")

    for version in set(re.findall(r"\bGLIBC_[\w.]+", details)):
        number = version.removeprefix("GLIBC_")
        if args.libc != "glibc" or not re.fullmatch(r"\d+(\.\d+)+", number):
            raise RuntimeError(f"Unexpected libc symbol version: {version}")
        if tuple(map(int, number.split("."))) > (2, 36):
            raise RuntimeError(f"Module requires {version}; the release baseline is glibc 2.36")

    for line in details.splitlines():
        fields = line.split()
        if (len(fields) >= 8 and fields[4] in {"GLOBAL", "WEAK"} and fields[6] != "UND"
                and fields[7].startswith(("pfm_", "cap_", "_cap_", "psx_"))):
            raise RuntimeError(f"Bundled dependency symbol must be private: {fields[7]}")

    files = {
        "perfidious.so": module,
        "LICENSE.md": Path("LICENSE.md"),
        "LICENSE_EXCEPTION.md": Path("docs/LICENSE_EXCEPTION.md"),
        "dependencies/libcap.tar.xz": Path("dependencies/libcap.tar.xz"),
        "dependencies/libpfm.tar.gz": Path("dependencies/libpfm.tar.gz"),
        "dependencies/README.txt": Path("dependencies/README.txt"),
    }
    for filename in files.values():
        if not filename.is_file():
            raise RuntimeError(f"Missing package input: {filename}")
    args.output_directory.mkdir(parents=True, exist_ok=True)
    archive = args.output_directory / f"php_perfidious-{args.tag}_php{php_version}-x86_64-linux-{args.libc}-nts.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for name, filename in files.items():
            entry = zipfile.ZipInfo(name)
            entry.external_attr = 0o100644 << 16
            package.writestr(entry, filename.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
    print(archive.resolve())


if __name__ == "__main__":
    main()
