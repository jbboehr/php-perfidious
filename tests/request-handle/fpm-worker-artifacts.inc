<?php

function perfidious_test_fpm_path(): ?string
{
    $candidate = dirname(PHP_BINARY) . '/php-fpm';
    if (!is_executable($candidate)) {
        return null;
    }

    return realpath($candidate) ?: $candidate;
}

function perfidious_test_module_path(): ?string
{
    $maps = @file('/proc/self/maps', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if ($maps === false) {
        return null;
    }

    foreach ($maps as $line) {
        if (preg_match('~\s(/.*?/perfidious\.so)(?: \(deleted\))?$~', $line, $matches) !== 1) {
            continue;
        }

        $candidate = $matches[1];
        if (is_file($candidate)) {
            return realpath($candidate) ?: $candidate;
        }
    }

    return null;
}
