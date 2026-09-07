#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && $1 != --check ) ]]; then
    echo 'usage: tools/generate-arginfo.sh [--check]' >&2
    exit 2
fi

: "${PERFIDIOUS_GEN_STUB:?Set PERFIDIOUS_GEN_STUB to PHP 8.5 gen_stub.php, or use nix run .#generate-arginfo}"
cd "$(dirname "$0")/.."

temporary=$(mktemp -d)
trap 'rm -f -- "$temporary"/*.stub.php "$temporary"/*_arginfo.h; rmdir "$temporary"' EXIT
platforms=(common linux darwin windows)
for platform in "${platforms[@]}"; do
    cp "stubs/$platform.stub.php" "$temporary/"
done

php "$PERFIDIOUS_GEN_STUB" --force-regeneration "$temporary"

status=0
for platform in "${platforms[@]}"; do
    generated="$temporary/${platform}_arginfo.h"
    target="src/${platform}_arginfo.h"
    if [[ ${1:-} == --check ]]; then
        if ! diff -u -- "$target" "$generated"; then
            echo "$target is not up to date; run nix run .#generate-arginfo" >&2
            status=1
        fi
    else
        cp "$generated" "$target"
    fi
done
exit "$status"
