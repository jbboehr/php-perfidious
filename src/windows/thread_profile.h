/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifndef PERFIDIOUS_WINDOWS_THREAD_PROFILE_H
#define PERFIDIOUS_WINDOWS_THREAD_PROFILE_H

#include "main/php.h"

#include "php_perfidious.h"

PERFIDIOUS_LOCAL DWORD perfidious_windows_thread_profile_enable(DWORD64 hardware_counter_mask, HANDLE *handle);

PERFIDIOUS_LOCAL DWORD
perfidious_windows_thread_profile_read(HANDLE handle, DWORD64 hardware_counter_mask, PERFORMANCE_DATA *data);

PERFIDIOUS_LOCAL DWORD perfidious_windows_thread_profile_disable(HANDLE handle);

#endif /* PERFIDIOUS_WINDOWS_THREAD_PROFILE_H */
