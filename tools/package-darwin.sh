#!/usr/bin/env bash
set -euo pipefail

tag="${1:?Usage: package-darwin.sh TAG OUTPUT_DIRECTORY}"
output_directory="${2:?Usage: package-darwin.sh TAG OUTPUT_DIRECTORY}"
module=modules/perfidious.so

if [[ "$(lipo -archs "$module")" != arm64 ]]; then
    echo "macOS packages require an ARM64-only module" >&2
    exit 1
fi

load_commands=$(otool -l "$module")
minimum_os=$(printf '%s\n' "$load_commands" | awk '$1 == "minos" { print $2 }')
if [[ "$minimum_os" != 11.0 && "$minimum_os" != 11.0.0 ]]; then
    echo "Expected a macOS 11.0 deployment target, got: $minimum_os" >&2
    exit 1
fi

dependencies=$(otool -L "$module")
while IFS= read -r dependency; do
    case "$dependency" in
        /usr/lib/*|/System/Library/*) ;;
        *) echo "Non-system dependency in macOS package: $dependency" >&2; exit 1 ;;
    esac
done < <(printf '%s\n' "$dependencies" | awk 'NR > 1 { print $1 }')

profile=$(php -r 'if (PHP_DEBUG) { throw new RuntimeException("Release packages require a non-debug PHP build"); } printf("%d.%d-arm64-darwin-bsdlibc-%s", PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_ZTS ? "zts" : "nts");')
mkdir -p "$output_directory"
output_directory=$(cd "$output_directory" && pwd)
archive="$output_directory/php_perfidious-${tag}_php${profile}.zip"
zip -j -q "$archive" "$module" LICENSE.md docs/LICENSE_EXCEPTION.md
printf '%s\n' "$archive"
