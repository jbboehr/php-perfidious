/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <libproc.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"

#include "php_perfidious.h"
#include "../sampler.h"
#include "resource_usage.h"

struct perfidious_platform_sampler
{
    uint32_t metrics;
    enum perfidious_scope_id scope;
    uint64_t thread_id;
};

static zend_result perfidious_darwin_store_cpu_time(
    uint64_t user_time_ns, uint64_t system_time_ns, struct perfidious_sampler_snapshot *snapshot
)
{
    if (UNEXPECTED(user_time_ns > UINT64_MAX - system_time_ns)) {
        zend_throw_exception(perfidious_overflow_exception_ce, "Darwin total CPU time overflow", 0);
        return FAILURE;
    }

    snapshot->values[PERFIDIOUS_METRIC_CPU_TIME] = user_time_ns + system_time_ns;
    return SUCCESS;
}

static zend_result perfidious_darwin_read_process_usage(struct rusage_info_v4 *usage)
{
    memset(usage, 0, sizeof(*usage));
    if (UNEXPECTED(proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *) usage) != 0)) {
        int error = errno;
        zend_throw_exception_ex(
            perfidious_io_exception_ce, error, "proc_pid_rusage failed: [%d] %s", error, strerror(error)
        );
        return FAILURE;
    }

    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_supported_metrics(
    uint32_t requested_metrics, enum perfidious_scope_id scope, uint32_t *supported_metrics
)
{
    const uint32_t base_process_metrics =
        PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK;
    const uint32_t nominal_process_metrics = base_process_metrics | PERFIDIOUS_METRIC_CPU_CYCLES_MASK;
    struct rusage_info_v4 usage;

    if (scope == PERFIDIOUS_SCOPE_CURRENT_THREAD) {
        *supported_metrics = PERFIDIOUS_METRIC_CPU_TIME_MASK;
        return SUCCESS;
    }

    ZEND_ASSERT(scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS);
    *supported_metrics = base_process_metrics;
    /* Let the common layer reject statically unsupported requests before this fallible host probe. */
    if (UNEXPECTED((requested_metrics & ~nominal_process_metrics) != 0)) {
        *supported_metrics = nominal_process_metrics;
        return SUCCESS;
    }
    if ((requested_metrics & PERFIDIOUS_METRIC_CPU_CYCLES_MASK) == 0) {
        return SUCCESS;
    }
    if (UNEXPECTED(FAILURE == perfidious_darwin_read_process_usage(&usage))) {
        return FAILURE;
    }
    if (usage.ri_cycles != 0) {
        *supported_metrics |= PERFIDIOUS_METRIC_CPU_CYCLES_MASK;
    }

    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_open(
    uint32_t metrics, enum perfidious_scope_id scope, struct perfidious_platform_sampler **sampler
)
{
    struct perfidious_platform_sampler *result;

    ZEND_ASSERT(scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS || scope == PERFIDIOUS_SCOPE_CURRENT_THREAD);
    result = ecalloc(1, sizeof(*result));
    result->metrics = metrics;
    result->scope = scope;
    if (scope == PERFIDIOUS_SCOPE_CURRENT_THREAD &&
        UNEXPECTED(FAILURE == perfidious_darwin_get_current_thread_id(&result->thread_id))) {
        efree(result);
        return FAILURE;
    }
    *sampler = result;

    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_read(
    struct perfidious_platform_sampler *sampler, struct perfidious_sampler_snapshot *snapshot
)
{
    memset(snapshot, 0, sizeof(*snapshot));

    if (sampler->scope == PERFIDIOUS_SCOPE_CURRENT_THREAD) {
        struct perfidious_darwin_thread_resource_usage usage;
        uint64_t thread_id;

        ZEND_ASSERT(sampler->metrics == PERFIDIOUS_METRIC_CPU_TIME_MASK);
        if (UNEXPECTED(FAILURE == perfidious_darwin_get_current_thread_id(&thread_id))) {
            return FAILURE;
        }
        if (UNEXPECTED(thread_id != sampler->thread_id)) {
            zend_throw_exception(
                perfidious_wrong_thread_exception_ce,
                "Darwin current-thread sampler must be read from the thread that opened it",
                0
            );
            return FAILURE;
        }
        if (UNEXPECTED(FAILURE == perfidious_darwin_read_current_thread_resource_usage(&usage))) {
            return FAILURE;
        }

        return perfidious_darwin_store_cpu_time(usage.user_time_ns, usage.system_time_ns, snapshot);
    }

    if ((sampler->metrics & (PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)) != 0) {
        struct rusage_info_v4 usage;

        if (UNEXPECTED(FAILURE == perfidious_darwin_read_process_usage(&usage))) {
            return FAILURE;
        }
        if ((sampler->metrics & PERFIDIOUS_METRIC_CPU_TIME_MASK) != 0) {
            uint64_t user_time_ns;
            uint64_t system_time_ns;

            if (!perfidious_darwin_mach_time_to_ns(usage.ri_user_time, &user_time_ns) ||
                !perfidious_darwin_mach_time_to_ns(usage.ri_system_time, &system_time_ns)) {
                return FAILURE;
            }
            if (UNEXPECTED(
                    FAILURE == perfidious_darwin_store_cpu_time(user_time_ns, system_time_ns, snapshot)
                )) {
                return FAILURE;
            }
        }
        if ((sampler->metrics & PERFIDIOUS_METRIC_CPU_CYCLES_MASK) != 0) {
            snapshot->values[PERFIDIOUS_METRIC_CPU_CYCLES] = usage.ri_cycles;
        }
    }

    if ((sampler->metrics & (PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK)) != 0) {
        struct rusage usage;

        memset(&usage, 0, sizeof(usage));
        if (UNEXPECTED(getrusage(RUSAGE_SELF, &usage) != 0)) {
            int error = errno;
            zend_throw_exception_ex(
                perfidious_io_exception_ce, error, "getrusage failed: [%d] %s", error, strerror(error)
            );
            return FAILURE;
        }
        if ((sampler->metrics & PERFIDIOUS_METRIC_PAGE_FAULTS_MASK) != 0) {
            if (UNEXPECTED(usage.ru_minflt < 0 || usage.ru_majflt < 0)) {
                zend_throw_exception(perfidious_io_exception_ce, "getrusage returned a negative page-fault count", 0);
                return FAILURE;
            }
            if (UNEXPECTED((uint64_t) usage.ru_minflt > UINT64_MAX - (uint64_t) usage.ru_majflt)) {
                zend_throw_exception(perfidious_overflow_exception_ce, "Darwin page-fault count overflow", 0);
                return FAILURE;
            }
            snapshot->values[PERFIDIOUS_METRIC_PAGE_FAULTS] = (uint64_t) usage.ru_minflt + (uint64_t) usage.ru_majflt;
        }

        if ((sampler->metrics & PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK) != 0) {
            if (UNEXPECTED(usage.ru_nvcsw < 0 || usage.ru_nivcsw < 0)) {
                zend_throw_exception(
                    perfidious_io_exception_ce, "getrusage returned a negative context-switch count", 0
                );
                return FAILURE;
            }
            if (UNEXPECTED((uint64_t) usage.ru_nvcsw > UINT64_MAX - (uint64_t) usage.ru_nivcsw)) {
                zend_throw_exception(perfidious_overflow_exception_ce, "Darwin context-switch count overflow", 0);
                return FAILURE;
            }
            snapshot->values[PERFIDIOUS_METRIC_CONTEXT_SWITCHES] =
                (uint64_t) usage.ru_nvcsw + (uint64_t) usage.ru_nivcsw;
        }
    }

    return SUCCESS;
}

PERFIDIOUS_LOCAL void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler)
{
    efree(sampler);
}
