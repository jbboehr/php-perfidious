<?php

function perfidious_skip_if_not_windows(string $osFamily): void
{
    if ($osFamily !== 'Windows') {
        die('skip: Windows only');
    }
}

function perfidious_skip_if_wine(string $feature): void
{
    if (getenv('PERFIDIOUS_TEST_WINE') === '1') {
        die('skip: Wine does not support ' . $feature);
    }
}

perfidious_skip_if_not_windows(PHP_OS_FAMILY);
