<?php

function perfidious_test_fpm_path(): ?string
{
    $candidate = dirname(PHP_BINARY) . '/php-fpm';
    if (!is_executable($candidate)) {
        return null;
    }

    return realpath($candidate) ?: $candidate;
}

function perfidious_test_fpm_module_path(): ?string
{
    if (getenv('PERFIDIOUS_TEST_FPM_BUILTIN') === '1') {
        return '';
    }

    return perfidious_test_module_path();
}

function perfidious_test_opcache_path(): ?string
{
    // PHP 8.5 builds OpCache into PHP; an empty path omits zend_extension in FPM.
    if (PHP_VERSION_ID >= 80500 && extension_loaded('Zend OPcache')) {
        return '';
    }

    $candidate = getenv('PERFIDIOUS_TEST_OPCACHE') ?: ini_get('extension_dir') . '/opcache.so';

    return is_file($candidate) ? $candidate : null;
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
