<?php

function perfidious_skip_if_not_linux(string $osFamily): void
{
    if ($osFamily !== 'Linux') {
        die('skip: Linux only');
    }
}

perfidious_skip_if_not_linux(PHP_OS_FAMILY);
