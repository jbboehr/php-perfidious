/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"
#include "php_perfidious.h"
#include "../sampler.h"
#include "counter_math.h"

static const struct
{
    uint32_t type;
    uint64_t config;
    const char *name;
} perfidious_linux_metrics[PERFIDIOUS_METRIC_COUNT] = {
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK,       "cpu-time"        },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS,      "page-faults"     },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES, "context-switches"},
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES,       "cpu-cycles"      },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS,     "instructions"    },
};

struct perfidious_linux_reading
{
    uint64_t value;
    uint64_t enabled;
    uint64_t running;
};

struct perfidious_linux_counter
{
    int fd;
    struct perfidious_linux_reading previous;
    uint64_t total;
};

struct perfidious_platform_sampler
{
    pid_t thread_id;
    struct perfidious_linux_counter counters[PERFIDIOUS_METRIC_COUNT];
};

static int perfidious_linux_open_metric(enum perfidious_metric_id metric, bool disabled)
{
    struct perf_event_attr attr = {
        .type = perfidious_linux_metrics[metric].type,
        .size = sizeof(attr),
        .config = perfidious_linux_metrics[metric].config,
        .disabled = disabled,
        .exclude_hv = 1,
        .read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING,
    };

    /* Independent events keep software accounting running when hardware events are multiplexed. */
    return (int) syscall(SYS_perf_event_open, &attr, 0, -1, -1, (unsigned long) PERF_FLAG_FD_CLOEXEC);
}

static zend_result perfidious_linux_sampler_error(const char *operation, enum perfidious_metric_id metric, int error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce,
        error,
        "%s failed for sampler metric %s: [%d] %s",
        operation,
        perfidious_linux_metrics[metric].name,
        error,
        strerror(error)
    );
    return FAILURE;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_supported_metrics(
    uint32_t requested_metrics, enum perfidious_scope_id scope, uint32_t *supported_metrics
)
{
    *supported_metrics = 0;
    if (scope != PERFIDIOUS_SCOPE_CURRENT_THREAD) {
        return SUCCESS;
    }
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        uint32_t mask = PERFIDIOUS_METRIC_MASK(metric);
        if ((requested_metrics & mask) == 0) {
            continue;
        }
        int fd = perfidious_linux_open_metric(metric, true);
        if (fd < 0) {
            int error = errno;
            if (error == ENOENT || error == ENODEV || error == EOPNOTSUPP || error == EINVAL) {
                continue;
            }
            return perfidious_linux_sampler_error("perf_event_open", metric, error);
        }
        close(fd);
        *supported_metrics |= mask;
    }
    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_open(
    uint32_t metrics, enum perfidious_scope_id scope, struct perfidious_platform_sampler **sampler
)
{
    ZEND_ASSERT(scope == PERFIDIOUS_SCOPE_CURRENT_THREAD);
    struct perfidious_platform_sampler *result = ecalloc(1, sizeof(*result));
    result->thread_id = (pid_t) syscall(SYS_gettid);
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        result->counters[metric].fd = -1;
    }
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        if ((metrics & PERFIDIOUS_METRIC_MASK(metric)) == 0) {
            continue;
        }
        result->counters[metric].fd = perfidious_linux_open_metric(metric, false);
        if (result->counters[metric].fd < 0) {
            int error = errno;
            perfidious_platform_sampler_close(result);
            return perfidious_linux_sampler_error("perf_event_open", metric, error);
        }
    }
    *sampler = result;
    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_read(
    struct perfidious_platform_sampler *sampler, struct perfidious_sampler_snapshot *snapshot
)
{
    struct perfidious_linux_counter next[PERFIDIOUS_METRIC_COUNT];
    if (UNEXPECTED((pid_t) syscall(SYS_gettid) != sampler->thread_id)) {
        zend_throw_exception(
            perfidious_wrong_thread_exception_ce,
            "Linux current-thread sampler must be read from the thread that opened it",
            0
        );
        return FAILURE;
    }
    memcpy(next, sampler->counters, sizeof(next));
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        struct perfidious_linux_counter *counter = &next[metric];
        struct perfidious_linux_reading reading;
        ssize_t length;
        uint64_t value, enabled, running, scaled;
        if (counter->fd < 0) {
            continue;
        }
        do {
            length = read(counter->fd, &reading, sizeof(reading));
        } while (length < 0 && errno == EINTR);
        if (UNEXPECTED(length != sizeof(reading))) {
            return perfidious_linux_sampler_error("read", metric, length < 0 ? errno : EIO);
        }
        if (UNEXPECTED(
                reading.value < counter->previous.value || reading.enabled < counter->previous.enabled ||
                reading.running < counter->previous.running
            )) {
            return perfidious_linux_sampler_error("counter monotonicity check", metric, EIO);
        }
        value = reading.value - counter->previous.value;
        enabled = reading.enabled - counter->previous.enabled;
        running = reading.running - counter->previous.running;
        if (UNEXPECTED(running > enabled || (running == 0 && (enabled != 0 || value != 0)))) {
            return perfidious_linux_sampler_error("counter running-time check", metric, EAGAIN);
        }
        scaled = 0;
        if (UNEXPECTED(
                (running != 0 && !perfidious_scale_uint64(value, enabled, running, &scaled)) ||
                scaled > UINT64_MAX - counter->total
            )) {
            zend_throw_exception_ex(
                perfidious_overflow_exception_ce,
                0,
                "Sampler %s counter overflow",
                perfidious_linux_metrics[metric].name
            );
            return FAILURE;
        }
        counter->total += scaled;
        counter->previous = reading;
    }
    /* A failed read must not consume part of an interval or publish a partial sample. */
    memcpy(sampler->counters, next, sizeof(next));
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        snapshot->values[metric] = next[metric].total;
    }
    return SUCCESS;
}

PERFIDIOUS_LOCAL void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler)
{
    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        if (sampler->counters[metric].fd >= 0) {
            /* Closing an inherited descriptor must not disable the parent's event. */
            close(sampler->counters[metric].fd);
        }
    }
    efree(sampler);
}
