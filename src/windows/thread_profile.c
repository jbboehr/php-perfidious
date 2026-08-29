/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <string.h>

#include "main/php.h"

#include "php_perfidious.h"
#include "thread_profile.h"

PERFIDIOUS_LOCAL DWORD perfidious_windows_thread_profile_enable(DWORD64 hardware_counter_mask, HANDLE *handle)
{
    return EnableThreadProfiling(GetCurrentThread(), THREAD_PROFILING_FLAG_DISPATCH, hardware_counter_mask, handle);
}

PERFIDIOUS_LOCAL DWORD
perfidious_windows_thread_profile_read(HANDLE handle, DWORD64 hardware_counter_mask, PERFORMANCE_DATA *data)
{
    DWORD flags = READ_THREAD_PROFILING_FLAG_DISPATCHING;

    memset(data, 0, sizeof(*data));
    data->Size = sizeof(*data);
    data->Version = PERFORMANCE_DATA_VERSION;

    if (hardware_counter_mask != 0) {
        flags |= READ_THREAD_PROFILING_FLAG_HARDWARE_COUNTERS;
    }

    return ReadThreadProfilingData(handle, flags, data);
}

PERFIDIOUS_LOCAL DWORD perfidious_windows_thread_profile_disable(HANDLE handle)
{
    return DisableThreadProfiling(handle);
}
