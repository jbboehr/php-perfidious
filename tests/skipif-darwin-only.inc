<?php

function perfidious_skip_if_not_darwin(string $osFamily): void
{
    if ($osFamily !== 'Darwin') {
        die('skip: Darwin only');
    }
}

perfidious_skip_if_not_darwin(PHP_OS_FAMILY);
