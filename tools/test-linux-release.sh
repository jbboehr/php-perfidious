#!/bin/sh
# Run inside the matching official PHP CLI container, with the checkout mounted at /source.
set -eu

archive="${1:?Usage: test-linux-release.sh ARCHIVE}"
work_directory=$(mktemp -d)
cp -R /source "$work_directory/source"
# PHP expands these variables, not the shell.
# shellcheck disable=SC2016
php -r '(new PharData($argv[1]))->extractTo($argv[2]);' "$archive" "$work_directory/package"
cmp /source/LICENSE.md "$work_directory/package/LICENSE.md"
cmp /source/docs/LICENSE_EXCEPTION.md "$work_directory/package/LICENSE_EXCEPTION.md"
module="$work_directory/package/perfidious.so"
php -n -d "extension=$module" /source/tools/check-linux-release.php

cd "$work_directory/source"
export TEST_PHP_EXECUTABLE
TEST_PHP_EXECUTABLE=$(command -v php)
export NO_INTERACTION=1 REPORT_EXIT_STATUS=1
if ! php -n /usr/local/lib/php/build/run-tests.php -q -n -d "extension=$module" tests; then
    find tests -type f \( -name '*.log' -o -name '*.diff' -o -name '*.mem' \) -print -exec cat {} \;
    exit 1
fi
