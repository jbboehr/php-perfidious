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

#include "main/php.h"
#include "Zend/zend_exceptions.h"

#include "php_perfidious.h"
#include "../sampler.h"

struct perfidious_platform_sampler
{
    uint32_t metrics;
};

static bool perfidious_timeval_to_ns(const struct timeval *value, uint64_t *result)
{
    uint64_t seconds;
    uint64_t microseconds;

    if (UNEXPECTED(value->tv_sec < 0 || value->tv_usec < 0)) {
        zend_throw_exception(perfidious_io_exception_ce, "getrusage returned a negative CPU time", 0);
        return false;
    }

    seconds = (uint64_t) value->tv_sec;
    microseconds = (uint64_t) value->tv_usec;
    if (UNEXPECTED(seconds > UINT64_MAX / UINT64_C(1000000000) || microseconds > UINT64_C(999999))) {
        zend_throw_exception(perfidious_overflow_exception_ce, "getrusage CPU time overflow", 0);
        return false;
    }
    *result = seconds * UINT64_C(1000000000) + microseconds * UINT64_C(1000);
    return true;
}

PERFIDIOUS_LOCAL uint32_t perfidious_platform_sampler_supported_metrics(enum perfidious_scope_id scope)
{
    if (scope != PERFIDIOUS_SCOPE_CURRENT_PROCESS) {
        return 0;
    }

    return PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK;
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
    struct rusage usage;
    uint64_t user_time_ns;
    uint64_t system_time_ns;

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&usage, 0, sizeof(usage));
    if (UNEXPECTED(getrusage(RUSAGE_SELF, &usage) != 0)) {
        int error = errno;
        zend_throw_exception_ex(perfidious_io_exception_ce, error, "getrusage failed: [%d] %s", error, strerror(error));
        return FAILURE;
    }

    if ((sampler->metrics & PERFIDIOUS_METRIC_CPU_TIME_MASK) != 0) {
        if (!perfidious_timeval_to_ns(&usage.ru_utime, &user_time_ns) ||
            !perfidious_timeval_to_ns(&usage.ru_stime, &system_time_ns)) {
            return FAILURE;
        }
        if (UNEXPECTED(user_time_ns > UINT64_MAX - system_time_ns)) {
            zend_throw_exception(perfidious_overflow_exception_ce, "getrusage total CPU time overflow", 0);
            return FAILURE;
        }
        snapshot->values[PERFIDIOUS_METRIC_CPU_TIME] = user_time_ns + system_time_ns;
    }

    if ((sampler->metrics & PERFIDIOUS_METRIC_PAGE_FAULTS_MASK) != 0) {
        if (UNEXPECTED(usage.ru_minflt < 0 || usage.ru_majflt < 0)) {
            zend_throw_exception(perfidious_io_exception_ce, "getrusage returned a negative page-fault count", 0);
            return FAILURE;
        }
        if (UNEXPECTED((uint64_t) usage.ru_minflt > UINT64_MAX - (uint64_t) usage.ru_majflt)) {
            zend_throw_exception(perfidious_overflow_exception_ce, "getrusage page-fault count overflow", 0);
            return FAILURE;
        }
        snapshot->values[PERFIDIOUS_METRIC_PAGE_FAULTS] = (uint64_t) usage.ru_minflt + (uint64_t) usage.ru_majflt;
    }

    return SUCCESS;
}

PERFIDIOUS_LOCAL void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler)
{
    efree(sampler);
}
