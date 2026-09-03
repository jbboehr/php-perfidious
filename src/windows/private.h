/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifndef PERFIDIOUS_WINDOWS_PRIVATE_H
#define PERFIDIOUS_WINDOWS_PRIVATE_H

#include <stdint.h>
#include <windows.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"

#include "php_perfidious.h"

static zend_always_inline uint64_t perfidious_windows_filetime_to_uint64(FILETIME value)
{
    ULARGE_INTEGER combined;

    combined.LowPart = value.dwLowDateTime;
    combined.HighPart = value.dwHighDateTime;
    return combined.QuadPart;
}

static zend_always_inline zend_result perfidious_windows_throw_error(const char *operation, DWORD error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce,
        (zend_long) error,
        "%s failed with Windows error %lu",
        operation,
        (unsigned long) error
    );
    return FAILURE;
}

#endif /* PERFIDIOUS_WINDOWS_PRIVATE_H */
