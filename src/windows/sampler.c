/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "main/php.h"
#include <psapi.h>
#include "Zend/zend_exceptions.h"

#include "php_perfidious.h"
#include "../sampler.h"

struct perfidious_platform_sampler
{
    uint32_t metrics;
    DWORD previous_page_faults;
    uint64_t page_fault_base;
    bool have_page_faults;
};

static uint64_t perfidious_windows_filetime_to_uint64(FILETIME value)
{
    ULARGE_INTEGER combined;

    combined.LowPart = value.dwLowDateTime;
    combined.HighPart = value.dwHighDateTime;
    return combined.QuadPart;
}

static zend_result perfidious_windows_throw_sampler_error(const char *operation, DWORD error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce, (zend_long) error, "%s failed with Windows error %lu", operation, error
    );
    return FAILURE;
}

PERFIDIOUS_LOCAL uint32_t perfidious_platform_sampler_supported_metrics(enum perfidious_scope_id scope)
{
    if (scope != PERFIDIOUS_SCOPE_CURRENT_PROCESS) {
        return 0;
    }

    return PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_open(
    uint32_t metrics, enum perfidious_scope_id scope, struct perfidious_platform_sampler **sampler
)
{
    struct perfidious_platform_sampler *result;

    ZEND_ASSERT(scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS);
    result = ecalloc(1, sizeof(*result));
    result->metrics = metrics;
    *sampler = result;

    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_read(
    struct perfidious_platform_sampler *sampler, struct perfidious_sampler_snapshot *snapshot
)
{
    memset(snapshot, 0, sizeof(*snapshot));

    if ((sampler->metrics & PERFIDIOUS_METRIC_CPU_TIME_MASK) != 0) {
        FILETIME creation_time;
        FILETIME exit_time;
        FILETIME kernel_time;
        FILETIME user_time;
        uint64_t kernel_time_100ns;
        uint64_t user_time_100ns;
        uint64_t total_time_100ns;

        if (UNEXPECTED(!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time))) {
            return perfidious_windows_throw_sampler_error("GetProcessTimes", GetLastError());
        }

        kernel_time_100ns = perfidious_windows_filetime_to_uint64(kernel_time);
        user_time_100ns = perfidious_windows_filetime_to_uint64(user_time);
        if (UNEXPECTED(kernel_time_100ns > UINT64_MAX - user_time_100ns)) {
            zend_throw_exception(perfidious_overflow_exception_ce, "Windows total CPU time overflow", 0);
            return FAILURE;
        }
        total_time_100ns = kernel_time_100ns + user_time_100ns;
        if (UNEXPECTED(total_time_100ns > UINT64_MAX / UINT64_C(100))) {
            zend_throw_exception(perfidious_overflow_exception_ce, "Windows CPU nanosecond conversion overflow", 0);
            return FAILURE;
        }
        snapshot->values[PERFIDIOUS_METRIC_CPU_TIME] = total_time_100ns * UINT64_C(100);
    }

    if ((sampler->metrics & PERFIDIOUS_METRIC_PAGE_FAULTS_MASK) != 0) {
        PROCESS_MEMORY_COUNTERS_EX memory;

        memset(&memory, 0, sizeof(memory));
        memory.cb = sizeof(memory);
        if (UNEXPECTED(
                !GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &memory, sizeof(memory))
            )) {
            return perfidious_windows_throw_sampler_error("GetProcessMemoryInfo", GetLastError());
        }

        if (sampler->have_page_faults && memory.PageFaultCount < sampler->previous_page_faults) {
            if (UNEXPECTED(sampler->page_fault_base > UINT64_MAX - (UINT64_C(1) << 32))) {
                zend_throw_exception(perfidious_overflow_exception_ce, "Windows page-fault count overflow", 0);
                return FAILURE;
            }
            sampler->page_fault_base += UINT64_C(1) << 32;
        }
        sampler->previous_page_faults = memory.PageFaultCount;
        sampler->have_page_faults = true;
        snapshot->values[PERFIDIOUS_METRIC_PAGE_FAULTS] = sampler->page_fault_base + memory.PageFaultCount;
    }

    if ((sampler->metrics & PERFIDIOUS_METRIC_CPU_CYCLES_MASK) != 0) {
        ULONG64 cycle_time;

        if (UNEXPECTED(!QueryProcessCycleTime(GetCurrentProcess(), &cycle_time))) {
            return perfidious_windows_throw_sampler_error("QueryProcessCycleTime", GetLastError());
        }
        snapshot->values[PERFIDIOUS_METRIC_CPU_CYCLES] = (uint64_t) cycle_time;
    }

    return SUCCESS;
}

PERFIDIOUS_LOCAL void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler)
{
    efree(sampler);
}
