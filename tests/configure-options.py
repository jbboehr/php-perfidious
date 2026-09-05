"""Check generated instrumentation settings under Dash and Bash without rebuilding PHP."""

import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def run(command, directory, environment):
    result = subprocess.run(
        command, cwd=directory, env=environment, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120,
    )
    if result.returncode:
        raise RuntimeError(f"{command!r} failed:\n{result.stdout}")
    return result.stdout


def check_options(directory, environment, expected):
    debug, coverage, sanitize = expected
    header = (directory / "config.h").read_text()
    commands = run(["make", "-n"], directory, environment).splitlines()
    compile_commands = [shlex.split(line) for line in commands if "--mode=compile" in line]
    link_commands = [shlex.split(line) for line in commands if "--mode=link" in line]
    if not compile_commands or not link_commands:
        raise RuntimeError("Generated build has no compile or link commands")

    errors = []
    for name, enabled in [("PERFIDIOUS_DEBUG", debug), ("NDEBUG", not debug)]:
        defined = re.search(rf"^#define\s+{name}\s+1\s*$", header, re.MULTILINE) is not None
        if defined != enabled:
            errors.append(f"{name}: expected {enabled}, got {defined}")
    for flag, enabled, recipes in [
        ("-fprofile-arcs", coverage, compile_commands),
        ("-ftest-coverage", coverage, compile_commands),
        ("--coverage", coverage, link_commands),
        ("-fsanitize=address,undefined", sanitize, compile_commands),
        ("-fno-sanitize-recover=all", sanitize, compile_commands),
        ("-fno-omit-frame-pointer", sanitize, compile_commands),
    ]:
        if any((flag in recipe) != enabled for recipe in recipes):
            errors.append(f"{flag}: expected {'present' if enabled else 'absent'} in every recipe")
    if errors:
        raise RuntimeError("; ".join(errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dash", default="dash", help="Dash executable (default: PATH lookup)")
    parser.add_argument("--bash", default="bash", help="Bash executable (default: PATH lookup)")
    args = parser.parse_args()
    shells = [shutil.which(args.dash), shutil.which(args.bash)]
    if not all(shells):
        parser.error("Both Dash and Bash are required; use --dash/--bash for explicit paths")

    root = Path(__file__).resolve().parents[1]
    environment = dict(os.environ, CFLAGS="-O0", CPPFLAGS="", LDFLAGS="", LIBS="")
    for name in ["LD_PRELOAD", "DYLD_INSERT_LIBRARIES", "PERFIDIOUS_EXTRA_CFLAGS"]:
        environment.pop(name, None)
    environment["CONFIG_SITE"] = "/dev/null"

    with tempfile.TemporaryDirectory(prefix="perfidious-configure-") as temporary:
        source = Path(temporary) / "source"
        source.mkdir()
        for name in ["config.m4", "php_perfidious.h"]:
            shutil.copyfile(root / name, source / name)
        shutil.copytree(root / "m4", source / "m4")
        for path in (root / "src").rglob("*"):
            if path.suffix in {".c", ".h"}:
                destination = source / path.relative_to(root)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, destination)
        run(["phpize"], source, environment)

        profiles = [
            ("enabled", (True, True, True)),
            ("disabled", (False, False, False)),
            ("debug-only", (True, False, False)),
            ("coverage-only", (False, True, False)),
            ("sanitize-only", (False, False, True)),
            ("defaults", None),
        ]
        for shell in shells:
            for name, settings in profiles:
                directory = Path(temporary) / f"{Path(shell).name}-{name}"
                directory.mkdir()
                options = [] if settings is None else [
                    f"--{'enable' if enabled else 'disable'}-perfidious-{option}"
                    for option, enabled in zip(["debug", "coverage", "sanitize"], settings)
                ]
                selected_environment = dict(environment, CONFIG_SHELL=shell, SHELL=shell)
                output = run([
                    shell, str(source / "configure"), "--enable-perfidious",
                    "--enable-compile-warnings=no", "--disable-Werror", *options,
                ], directory, selected_environment)
                try:
                    check_options(directory, selected_environment, settings or (False, False, False))
                except RuntimeError as error:
                    raise RuntimeError(f"{Path(shell).name} {name}: {error}\n{output}") from error
                print(f"{Path(shell).name} {name}: passed", flush=True)


if __name__ == "__main__":
    main()
