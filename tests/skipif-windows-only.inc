<?php

function perfidious_skip_if_not_windows(string $osFamily): void
{
    if ($osFamily !== 'Windows') {
        die('skip: Windows only');
    }
}

perfidious_skip_if_not_windows(PHP_OS_FAMILY);
