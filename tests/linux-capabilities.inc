<?php

function perfidious_linux_status_has_effective_capability(string $status, int $capability): ?bool
{
    $hex = null;
    foreach (explode("\n", $status) as $line) {
        if (str_starts_with($line, 'CapEff:')) {
            $hex = trim(substr($line, strlen('CapEff:')));
            break;
        }
    }

    if ($hex === null || $hex === '' || strspn($hex, '0123456789abcdefABCDEF') !== strlen($hex)) {
        return null;
    }

    $nibbleIndex = strlen($hex) - intdiv($capability, 4) - 1;
    if ($nibbleIndex < 0) {
        return false;
    }

    $nibble = hexdec($hex[$nibbleIndex]);
    return ($nibble & (1 << ($capability % 4))) !== 0;
}
