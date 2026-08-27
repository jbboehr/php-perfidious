/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3,
 * as published by the Free Software Foundation, together with the Romic
 * Exception (an additional permission under section 7 of that license).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * and the Romic Exception along with this program.  If not, see
 * <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main/php.h"
#include <psapi.h>
#include "Zend/zend_API.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_long.h"

#include "php_perfidious.h"

#define PHP_PERFIDIOUS_WINDOWS_NAMESPACE PHP_PERFIDIOUS_NAMESPACE "\\Windows"
#define PERFIDIOUS_WINDOWS_HARDWARE_COUNTER_MASK_MAX 0xffff

struct perfidious_windows_thread_profile_obj
{
    HANDLE handle;
    DWORD64 hardware_counter_mask;
    uint64_t cycle_time_origin;
    uint64_t hardware_counter_origins[MAX_HW_COUNTERS];
    zend_object std;
};

static zend_class_entry *perfidious_windows_thread_profile_ce;
static zend_class_entry *perfidious_windows_process_times_ce;
static zend_class_entry *perfidious_windows_process_memory_info_ce;
static zend_class_entry *perfidious_windows_thread_profile_snapshot_ce;
static zend_class_entry *perfidious_windows_hardware_counter_snapshot_ce;
static zend_object_handlers perfidious_windows_thread_profile_obj_handlers;

// clang-format off
ZEND_BEGIN_ARG_INFO_EX(perfidious_windows_result_construct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousWindowsResult, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

static const zend_function_entry perfidious_windows_result_methods[] = {
    PHP_ME(PerfidiousWindowsResult, __construct, perfidious_windows_result_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_FE_END
};
// clang-format on

static void perfidious_windows_declare_readonly_property(
    zend_class_entry *class_entry, const char *name, size_t name_length, uint32_t type_mask
)
{
    zend_string *property_name = zend_string_init_interned(name, name_length, true);
    zval default_value;

    ZVAL_UNDEF(&default_value);
    zend_declare_typed_property(
        class_entry,
        property_name,
        &default_value,
        ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
        NULL,
        (zend_type) ZEND_TYPE_INIT_MASK(type_mask)
    );
}

#define PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(class_entry, name, type_mask)                                     \
    perfidious_windows_declare_readonly_property(class_entry, ZEND_STRL(name), type_mask)

static zend_class_entry *perfidious_windows_register_result_class(const char *name, size_t name_length)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY_EX(ce, name, name_length, perfidious_windows_result_methods);
    class_entry = zend_register_internal_class(&ce);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    return class_entry;
}

static void perfidious_windows_register_process_result_classes(void)
{
    perfidious_windows_process_times_ce =
        perfidious_windows_register_result_class(ZEND_STRL(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\ProcessTimes"));
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_times_ce, "creationTimeFiletime", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(perfidious_windows_process_times_ce, "kernelTime100ns", MAY_BE_LONG);
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(perfidious_windows_process_times_ce, "userTime100ns", MAY_BE_LONG);

    perfidious_windows_process_memory_info_ce =
        perfidious_windows_register_result_class(ZEND_STRL(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\ProcessMemoryInfo"));
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "pageFaultCount", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "peakWorkingSetSize", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "workingSetSize", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "quotaPeakPagedPoolUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "quotaPagedPoolUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "quotaPeakNonPagedPoolUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "quotaNonPagedPoolUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "pagefileUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "peakPagefileUsage", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_process_memory_info_ce, "privateUsage", MAY_BE_LONG
    );
}

static void perfidious_windows_register_thread_result_classes(void)
{
    perfidious_windows_hardware_counter_snapshot_ce = perfidious_windows_register_result_class(
        ZEND_STRL(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\HardwareCounterSnapshot")
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(perfidious_windows_hardware_counter_snapshot_ce, "index", MAY_BE_LONG);
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(perfidious_windows_hardware_counter_snapshot_ce, "type", MAY_BE_LONG);
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(perfidious_windows_hardware_counter_snapshot_ce, "value", MAY_BE_LONG);

    perfidious_windows_thread_profile_snapshot_ce =
        perfidious_windows_register_result_class(ZEND_STRL(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\ThreadProfileSnapshot"));
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "contextSwitchCount", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "waitReasonBitmapHex", MAY_BE_STRING
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "cycleCount", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "readRetryCount", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "hardwareCounterCount", MAY_BE_LONG
    );
    PERFIDIOUS_WINDOWS_DECLARE_READONLY_PROPERTY(
        perfidious_windows_thread_profile_snapshot_ce, "hardwareCounters", MAY_BE_ARRAY
    );
}

static inline struct perfidious_windows_thread_profile_obj *
perfidious_windows_fetch_thread_profile_object(zend_object *obj)
{
    return (struct perfidious_windows_thread_profile_obj *) ((char *) obj -
                                                             XtOffsetOf(
                                                                 struct perfidious_windows_thread_profile_obj, std
                                                             ));
}

static void perfidious_windows_throw_error(const char *function_name, DWORD error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce,
        (zend_long) error,
        "%s failed with Windows error %lu",
        function_name,
        (unsigned long) error
    );
}

static bool perfidious_windows_uint64_to_zend_long(uint64_t value, zend_long *result)
{
    if (UNEXPECTED(value > (uint64_t) ZEND_LONG_MAX)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "Windows counter value 0x%016llx is too large to represent as a PHP integer",
            (unsigned long long) value
        );
        return false;
    }

    *result = (zend_long) value;
    return true;
}

static uint64_t perfidious_windows_filetime_to_uint64(FILETIME value)
{
    ULARGE_INTEGER combined;

    combined.LowPart = value.dwLowDateTime;
    combined.HighPart = value.dwHighDateTime;

    return combined.QuadPart;
}

static DWORD
perfidious_windows_read_thread_profile(HANDLE handle, DWORD64 hardware_counter_mask, PERFORMANCE_DATA *data)
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

static void perfidious_windows_thread_profile_obj_free(zend_object *object)
{
    struct perfidious_windows_thread_profile_obj *obj = perfidious_windows_fetch_thread_profile_object(object);

    if (obj->handle != NULL) {
        DisableThreadProfiling(obj->handle);
        obj->handle = NULL;
    }

    zend_object_std_dtor(object);
}

static zend_object *perfidious_windows_thread_profile_obj_create(zend_class_entry *ce)
{
    struct perfidious_windows_thread_profile_obj *obj =
        zend_object_alloc(sizeof(struct perfidious_windows_thread_profile_obj), ce);

    obj->handle = NULL;
    obj->hardware_counter_mask = 0;
    obj->cycle_time_origin = 0;
    memset(obj->hardware_counter_origins, 0, sizeof(obj->hardware_counter_origins));
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &perfidious_windows_thread_profile_obj_handlers;

    return &obj->std;
}

// clang-format off
ZEND_BEGIN_ARG_INFO_EX(perfidious_windows_thread_profile_construct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousWindowsThreadProfile, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}
// clang-format on

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_windows_thread_profile_read_arginfo, false, 0, Perfidious\\Windows\\ThreadProfileSnapshot, false
)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousWindowsThreadProfile, read)
{
    struct perfidious_windows_thread_profile_obj *obj;
    PERFORMANCE_DATA data;
    DWORD error;
    zend_long cycle_time;
    zend_long hardware_counter_values[MAX_HW_COUNTERS];
    char wait_reason_bitmap[19];

    ZEND_PARSE_PARAMETERS_NONE();

    obj = perfidious_windows_fetch_thread_profile_object(Z_OBJ_P(ZEND_THIS));
    if (UNEXPECTED(obj->handle == NULL)) {
        zend_throw_exception(perfidious_io_exception_ce, "Thread profile is closed", ERROR_INVALID_HANDLE);
        return;
    }

    error = perfidious_windows_read_thread_profile(obj->handle, obj->hardware_counter_mask, &data);
    if (UNEXPECTED(error != ERROR_SUCCESS)) {
        perfidious_windows_throw_error("ReadThreadProfilingData", error);
        return;
    }

    if (UNEXPECTED(data.HwCountersCount > MAX_HW_COUNTERS)) {
        zend_throw_exception(perfidious_io_exception_ce, "Windows returned too many hardware counters", 0);
        return;
    }

    if (!perfidious_windows_uint64_to_zend_long(data.CycleTime - obj->cycle_time_origin, &cycle_time)) {
        return;
    }

    snprintf(wait_reason_bitmap, sizeof(wait_reason_bitmap), "0x%016llx", (unsigned long long) data.WaitReasonBitMap);

    for (BYTE i = 0; i < MAX_HW_COUNTERS; i++) {
        if ((obj->hardware_counter_mask & (((DWORD64) 1) << i)) != 0 &&
            !perfidious_windows_uint64_to_zend_long(
                data.HwCounters[i].Value - obj->hardware_counter_origins[i], &hardware_counter_values[i]
            )) {
            return;
        }
    }

    zval hardware_counters;
    array_init(&hardware_counters);

    for (BYTE i = 0; i < MAX_HW_COUNTERS; i++) {
        zval hardware_counter;

        if ((obj->hardware_counter_mask & (((DWORD64) 1) << i)) == 0) {
            continue;
        }

        object_init_ex(&hardware_counter, perfidious_windows_hardware_counter_snapshot_ce);
        zend_update_property_long(
            perfidious_windows_hardware_counter_snapshot_ce, Z_OBJ(hardware_counter), ZEND_STRL("index"), (zend_long) i
        );
        zend_update_property_long(
            perfidious_windows_hardware_counter_snapshot_ce,
            Z_OBJ(hardware_counter),
            ZEND_STRL("type"),
            (zend_long) data.HwCounters[i].Type
        );
        zend_update_property_long(
            perfidious_windows_hardware_counter_snapshot_ce,
            Z_OBJ(hardware_counter),
            ZEND_STRL("value"),
            hardware_counter_values[i]
        );
        add_index_zval(&hardware_counters, (zend_long) i, &hardware_counter);
    }

    object_init_ex(return_value, perfidious_windows_thread_profile_snapshot_ce);
    zend_update_property_long(
        perfidious_windows_thread_profile_snapshot_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("contextSwitchCount"),
        (zend_long) data.ContextSwitchCount
    );
    zend_update_property_string(
        perfidious_windows_thread_profile_snapshot_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("waitReasonBitmapHex"),
        wait_reason_bitmap
    );
    zend_update_property_long(
        perfidious_windows_thread_profile_snapshot_ce, Z_OBJ_P(return_value), ZEND_STRL("cycleCount"), cycle_time
    );
    zend_update_property_long(
        perfidious_windows_thread_profile_snapshot_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("readRetryCount"),
        (zend_long) data.RetryCount
    );
    zend_update_property_long(
        perfidious_windows_thread_profile_snapshot_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("hardwareCounterCount"),
        (zend_long) data.HwCountersCount
    );
    zend_update_property(
        perfidious_windows_thread_profile_snapshot_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("hardwareCounters"),
        &hardware_counters
    );
    zval_ptr_dtor(&hardware_counters);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(perfidious_windows_thread_profile_close_arginfo, false, 0, IS_VOID, false)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousWindowsThreadProfile, close)
{
    struct perfidious_windows_thread_profile_obj *obj;
    DWORD error;

    ZEND_PARSE_PARAMETERS_NONE();

    obj = perfidious_windows_fetch_thread_profile_object(Z_OBJ_P(ZEND_THIS));
    if (obj->handle == NULL) {
        return;
    }

    error = DisableThreadProfiling(obj->handle);
    if (UNEXPECTED(error != ERROR_SUCCESS)) {
        perfidious_windows_throw_error("DisableThreadProfiling", error);
        return;
    }

    obj->handle = NULL;
}

// clang-format off
static const zend_function_entry perfidious_windows_thread_profile_methods[] = {
    PHP_ME(PerfidiousWindowsThreadProfile, __construct, perfidious_windows_thread_profile_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_ME(PerfidiousWindowsThreadProfile, read, perfidious_windows_thread_profile_read_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidiousWindowsThreadProfile, close, perfidious_windows_thread_profile_close_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_FE_END
};
// clang-format on

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    perfidious_windows_query_current_process_cycle_time_arginfo, false, 0, IS_LONG, false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_windows_query_current_process_cycle_time)
{
    ULONG64 value;
    zend_long result;

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!QueryProcessCycleTime(GetCurrentProcess(), &value))) {
        perfidious_windows_throw_error("QueryProcessCycleTime", GetLastError());
        return;
    }

    if (!perfidious_windows_uint64_to_zend_long(value, &result)) {
        return;
    }

    RETURN_LONG(result);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    perfidious_windows_query_current_thread_cycle_time_arginfo, false, 0, IS_LONG, false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_windows_query_current_thread_cycle_time)
{
    ULONG64 value;
    zend_long result;

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!QueryThreadCycleTime(GetCurrentThread(), &value))) {
        perfidious_windows_throw_error("QueryThreadCycleTime", GetLastError());
        return;
    }

    if (!perfidious_windows_uint64_to_zend_long(value, &result)) {
        return;
    }

    RETURN_LONG(result);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_windows_get_current_process_times_arginfo, false, 0, Perfidious\\Windows\\ProcessTimes, false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_windows_get_current_process_times)
{
    FILETIME creation_time;
    FILETIME unused_exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    zend_long values[3];

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(
            !GetProcessTimes(GetCurrentProcess(), &creation_time, &unused_exit_time, &kernel_time, &user_time)
        )) {
        perfidious_windows_throw_error("GetProcessTimes", GetLastError());
        return;
    }

    if (!perfidious_windows_uint64_to_zend_long(perfidious_windows_filetime_to_uint64(creation_time), &values[0]) ||
        !perfidious_windows_uint64_to_zend_long(perfidious_windows_filetime_to_uint64(kernel_time), &values[1]) ||
        !perfidious_windows_uint64_to_zend_long(perfidious_windows_filetime_to_uint64(user_time), &values[2])) {
        return;
    }

    object_init_ex(return_value, perfidious_windows_process_times_ce);
    zend_update_property_long(
        perfidious_windows_process_times_ce, Z_OBJ_P(return_value), ZEND_STRL("creationTimeFiletime"), values[0]
    );
    zend_update_property_long(
        perfidious_windows_process_times_ce, Z_OBJ_P(return_value), ZEND_STRL("kernelTime100ns"), values[1]
    );
    zend_update_property_long(
        perfidious_windows_process_times_ce, Z_OBJ_P(return_value), ZEND_STRL("userTime100ns"), values[2]
    );
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_windows_get_current_process_memory_info_arginfo, false, 0, Perfidious\\Windows\\ProcessMemoryInfo, false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_windows_get_current_process_memory_info)
{
    PROCESS_MEMORY_COUNTERS_EX memory;
    zend_long values[10];

    ZEND_PARSE_PARAMETERS_NONE();

    memset(&memory, 0, sizeof(memory));
    memory.cb = sizeof(memory);

    if (UNEXPECTED(!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &memory, sizeof(memory)))) {
        perfidious_windows_throw_error("GetProcessMemoryInfo", GetLastError());
        return;
    }

    if (!perfidious_windows_uint64_to_zend_long(memory.PageFaultCount, &values[0]) ||
        !perfidious_windows_uint64_to_zend_long(memory.PeakWorkingSetSize, &values[1]) ||
        !perfidious_windows_uint64_to_zend_long(memory.WorkingSetSize, &values[2]) ||
        !perfidious_windows_uint64_to_zend_long(memory.QuotaPeakPagedPoolUsage, &values[3]) ||
        !perfidious_windows_uint64_to_zend_long(memory.QuotaPagedPoolUsage, &values[4]) ||
        !perfidious_windows_uint64_to_zend_long(memory.QuotaPeakNonPagedPoolUsage, &values[5]) ||
        !perfidious_windows_uint64_to_zend_long(memory.QuotaNonPagedPoolUsage, &values[6]) ||
        !perfidious_windows_uint64_to_zend_long(memory.PagefileUsage, &values[7]) ||
        !perfidious_windows_uint64_to_zend_long(memory.PeakPagefileUsage, &values[8]) ||
        !perfidious_windows_uint64_to_zend_long(memory.PrivateUsage, &values[9])) {
        return;
    }

    object_init_ex(return_value, perfidious_windows_process_memory_info_ce);
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("pageFaultCount"), values[0]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("peakWorkingSetSize"), values[1]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("workingSetSize"), values[2]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("quotaPeakPagedPoolUsage"),
        values[3]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("quotaPagedPoolUsage"), values[4]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("quotaPeakNonPagedPoolUsage"),
        values[5]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("quotaNonPagedPoolUsage"), values[6]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("pagefileUsage"), values[7]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("peakPagefileUsage"), values[8]
    );
    zend_update_property_long(
        perfidious_windows_process_memory_info_ce, Z_OBJ_P(return_value), ZEND_STRL("privateUsage"), values[9]
    );
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_windows_enable_current_thread_profiling_arginfo, false, 0, Perfidious\\Windows\\ThreadProfile, false
)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(false, hardwareCounterMask, IS_LONG, false, "0")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_windows_enable_current_thread_profiling)
{
    zend_long hardware_counters = 0;
    HANDLE handle;
    DWORD error;
    PERFORMANCE_DATA initial_data;
    struct perfidious_windows_thread_profile_obj *obj;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(hardware_counters)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(hardware_counters < 0 || hardware_counters > PERFIDIOUS_WINDOWS_HARDWARE_COUNTER_MASK_MAX)) {
        zend_argument_value_error(
            1, "must be between 0 and %u", (unsigned int) PERFIDIOUS_WINDOWS_HARDWARE_COUNTER_MASK_MAX
        );
        return;
    }

    error =
        EnableThreadProfiling(GetCurrentThread(), THREAD_PROFILING_FLAG_DISPATCH, (DWORD64) hardware_counters, &handle);
    if (UNEXPECTED(error != ERROR_SUCCESS)) {
        perfidious_windows_throw_error("EnableThreadProfiling", error);
        return;
    }

    error = perfidious_windows_read_thread_profile(handle, (DWORD64) hardware_counters, &initial_data);
    if (UNEXPECTED(error != ERROR_SUCCESS)) {
        DisableThreadProfiling(handle);
        perfidious_windows_throw_error("ReadThreadProfilingData", error);
        return;
    }

    object_init_ex(return_value, perfidious_windows_thread_profile_ce);
    obj = perfidious_windows_fetch_thread_profile_object(Z_OBJ_P(return_value));
    obj->handle = handle;
    obj->hardware_counter_mask = (DWORD64) hardware_counters;
    // HCP can expose an unsigned implementation-specific baseline (notably under virtualization),
    // so normalize it to the API's documented "since profiling was enabled" meaning.
    obj->cycle_time_origin = initial_data.CycleTime;

    for (BYTE i = 0; i < MAX_HW_COUNTERS; i++) {
        if ((obj->hardware_counter_mask & (((DWORD64) 1) << i)) != 0) {
            obj->hardware_counter_origins[i] = initial_data.HwCounters[i].Value;
        }
    }
}

#if PHP_VERSION_ID >= 80400
#define PERFIDIOUS_WINDOWS_FE(zend_name, name, arg_info, flags)                                                        \
    ZEND_RAW_FENTRY(zend_name, name, arg_info, flags, NULL, NULL)
#else
#define PERFIDIOUS_WINDOWS_FE(zend_name, name, arg_info, flags) ZEND_RAW_FENTRY(zend_name, name, arg_info, flags)
#endif

// clang-format off
PERFIDIOUS_LOCAL
const zend_function_entry perfidious_windows_functions[] = {
    PERFIDIOUS_WINDOWS_FE(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\query_current_process_cycle_time", ZEND_FN(perfidious_windows_query_current_process_cycle_time), perfidious_windows_query_current_process_cycle_time_arginfo, 0)
    PERFIDIOUS_WINDOWS_FE(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\query_current_thread_cycle_time", ZEND_FN(perfidious_windows_query_current_thread_cycle_time), perfidious_windows_query_current_thread_cycle_time_arginfo, 0)
    PERFIDIOUS_WINDOWS_FE(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\get_current_process_times", ZEND_FN(perfidious_windows_get_current_process_times), perfidious_windows_get_current_process_times_arginfo, 0)
    PERFIDIOUS_WINDOWS_FE(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\get_current_process_memory_info", ZEND_FN(perfidious_windows_get_current_process_memory_info), perfidious_windows_get_current_process_memory_info_arginfo, 0)
    PERFIDIOUS_WINDOWS_FE(PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\enable_current_thread_profiling", ZEND_FN(perfidious_windows_enable_current_thread_profiling), perfidious_windows_enable_current_thread_profiling_arginfo, 0)
    PHP_FE_END
};
// clang-format on

PERFIDIOUS_LOCAL void perfidious_windows_minit(void)
{
    zend_class_entry ce;

    perfidious_windows_register_process_result_classes();
    perfidious_windows_register_thread_result_classes();

    memcpy(
        &perfidious_windows_thread_profile_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)
    );
    perfidious_windows_thread_profile_obj_handlers.offset =
        XtOffsetOf(struct perfidious_windows_thread_profile_obj, std);
    perfidious_windows_thread_profile_obj_handlers.free_obj = perfidious_windows_thread_profile_obj_free;
    perfidious_windows_thread_profile_obj_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_WINDOWS_NAMESPACE "\\ThreadProfile", perfidious_windows_thread_profile_methods);
    perfidious_windows_thread_profile_ce = zend_register_internal_class(&ce);
    perfidious_windows_thread_profile_ce->ce_flags |=
        ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    perfidious_windows_thread_profile_ce->create_object = perfidious_windows_thread_profile_obj_create;
}
