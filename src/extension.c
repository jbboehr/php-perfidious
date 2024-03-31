/**
 * Copyright (C) 2024 John Boehr & contributors
 *
 * This file is part of php-perf.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <inttypes.h>
#include <err.h>
#include <perfmon/pfmlib.h>

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_ini.h"
#include "Zend/zend_modules.h"
#include "Zend/zend_operators.h"
#include "main/php.h"
#include "main/php_ini.h"
#include "main/SAPI.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "php_perf.h"
#include "./functions.h"

#define DEFAULT_METRICS "PERF_FLAG_HW_CPU_CYCLES,PERF_FLAG_HW_INSTRUCTIONS"

ZEND_DECLARE_MODULE_GLOBALS(perf);

PERF_LOCAL zend_result php_perf_handle_minit(void);
PERF_LOCAL zend_result php_perf_pmu_enum_minit(void);

PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("perf.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, enable, zend_perf_globals, perf_globals)
STD_PHP_INI_ENTRY(
    "perf.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateString, enable, zend_perf_globals, perf_globals
)
PHP_INI_END()

struct read_format
{
    uint64_t nr;
    struct
    {
        uint64_t value;
        uint64_t id;
    } values[];
};

enum php_perf_metric_flag
{
    PERF_FLAG_HW_CPU_CYCLES = 100,
    PERF_FLAG_HW_INSTRUCTIONS = 101,
    PERF_FLAG_HW_CACHE_REFERENCES = 102,
    PERF_FLAG_HW_CACHE_MISSES = 103,
    PERF_FLAG_HW_BRANCH_INSTRUCTIONS = 104,
    PERF_FLAG_HW_BRANCH_MISSES = 105,
    PERF_FLAG_HW_BUS_CYCLES = 106,
    PERF_FLAG_HW_STALLED_CYCLES_FRONTEND = 107,
    PERF_FLAG_HW_STALLED_CYCLES_BACKEND = 108,
    PERF_FLAG_HW_REF_CPU_CYCLES = 109,

    PERF_FLAG_SW_CPU_CLOCK = 200,
    PERF_FLAG_SW_TASK_CLOCK = 201,
    PERF_FLAG_SW_PAGE_FAULTS = 202,
    PERF_FLAG_SW_CONTEXT_SWITCHES = 203,
    PERF_FLAG_SW_CPU_MIGRATIONS = 204,
    PERF_FLAG_SW_PAGE_FAULTS_MIN = 205,
    PERF_FLAG_SW_PAGE_FAULTS_MAJ = 206,
    PERF_FLAG_SW_ALIGNMENT_FAULTS = 207,
    PERF_FLAG_SW_EMULATION_FAULTS = 208,
    PERF_FLAG_SW_DUMMY = 209,
    PERF_FLAG_SW_BPF_OUTPUT = 210,
    PERF_FLAG_SW_CGROUP_SWITCHES = 211,
};

struct php_perf_available_metric
{
    uint32_t type;
    uint32_t config;
    enum php_perf_metric_flag flag;
    const char *name;
    size_t name_len;
};

struct php_perf_enabled_metric
{
    struct php_perf_available_metric *avail;
    int fd;
    uint64_t id;
};

#define MFNL(metric) metric, ZEND_STRL(#metric)

static struct php_perf_available_metric available_metrics[] = {
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES,              MFNL(PERF_FLAG_HW_CPU_CYCLES)             },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS,            MFNL(PERF_FLAG_HW_INSTRUCTIONS)           },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES,        MFNL(PERF_FLAG_HW_CACHE_REFERENCES)       },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES,            MFNL(PERF_FLAG_HW_CACHE_MISSES)           },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS,     MFNL(PERF_FLAG_HW_BRANCH_INSTRUCTIONS)    },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES,           MFNL(PERF_FLAG_HW_BRANCH_MISSES)          },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES,              MFNL(PERF_FLAG_HW_BUS_CYCLES)             },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, MFNL(PERF_FLAG_HW_STALLED_CYCLES_FRONTEND)},
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND,  MFNL(PERF_FLAG_HW_STALLED_CYCLES_BACKEND) },
    {PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES,          MFNL(PERF_FLAG_HW_REF_CPU_CYCLES)         },

    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK,               MFNL(PERF_FLAG_SW_CPU_CLOCK)              },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK,              MFNL(PERF_FLAG_SW_TASK_CLOCK)             },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS,             MFNL(PERF_FLAG_SW_PAGE_FAULTS)            },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES,        MFNL(PERF_FLAG_SW_CONTEXT_SWITCHES)       },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS,          MFNL(PERF_FLAG_SW_CPU_MIGRATIONS)         },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN,         MFNL(PERF_FLAG_SW_PAGE_FAULTS_MIN)        },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ,         MFNL(PERF_FLAG_SW_PAGE_FAULTS_MAJ)        },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS,        MFNL(PERF_FLAG_SW_ALIGNMENT_FAULTS)       },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS,        MFNL(PERF_FLAG_SW_EMULATION_FAULTS)       },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_DUMMY,                   MFNL(PERF_FLAG_SW_DUMMY)                  },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT,              MFNL(PERF_FLAG_SW_BPF_OUTPUT)             },
    {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CGROUP_SWITCHES,         MFNL(PERF_FLAG_SW_CGROUP_SWITCHES)        },
};

static size_t available_metrics_count = sizeof(available_metrics) / sizeof(struct php_perf_available_metric);

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    long ret;

    ret = syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);

    return ret;
}

static int perf_event_open_simple(__u32 type, __u64 config, int group_fd)
{
    int fd;
    pid_t pid = 0;
    int cpu = -1;
    unsigned long flags = 0;
    struct perf_event_attr pe;

    memset(&pe, 0, sizeof(pe));
    pe.type = type;
    pe.size = sizeof(pe);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    fd = (int) perf_event_open(&pe, pid, cpu, group_fd, flags);
    if (fd == -1) {
        php_error_docref(
            NULL,
            E_WARNING,
            "perf_event_open: failed for (%x/%llx): %s (%d)",
            pe.type,
            pe.config,
            strerror(errno),
            errno
        );
        return -1;
    }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);

    return fd;
}

static inline void unregister_events(void)
{
    if (PERF_G(enabled_metrics)) {
        for (int i = PERF_G(enabled_metrics_count); i >= 0; i--) {
            close(PERF_G(enabled_metrics)[i].fd);
        }

        pefree(PERF_G(enabled_metrics), 1);
        PERF_G(enabled_metrics) = NULL;
        PERF_G(enabled_metrics_count) = 0;
    }
}

static inline int register_events(void)
{
    if (!PERF_G(enable) || !PERF_G(metrics)) {
        return SUCCESS;
    }

    // Parse metrics string
    do {
        zval z_metrics = {0};
        array_init(&z_metrics);

        zend_string *delim = zend_string_init_fast(ZEND_STRL(","));
        zend_string *orig = zend_string_init(PERF_G(metrics), strlen(PERF_G(metrics)), 0);
        php_explode(delim, orig, &z_metrics, 0);
        zend_string_release(delim);
        zend_string_release(orig);

        if (Z_TYPE(z_metrics) != IS_ARRAY) {
            zval_dtor(&z_metrics);
            return FAILURE;
        }

        // just allocate big enough for all metrics
        struct php_perf_enabled_metric *enabled_metrics =
            pecalloc(sizeof(struct php_perf_enabled_metric), available_metrics_count + 1, 1);
        size_t enabled_metrics_count = 0;

        enabled_metrics[enabled_metrics_count++] = (struct php_perf_enabled_metric){
            .avail = NULL,
            .id = 0,
            .fd = -1,
        };

        zval *z;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(z_metrics), z)
        {
            if (Z_TYPE_P(z) != IS_STRING) {
                continue;
            }

            for (size_t i = 0; i < available_metrics_count; i++) {
                struct php_perf_available_metric *a = &available_metrics[i];
                if (Z_STRLEN_P(z) == a->name_len && 0 == memcmp(a->name, Z_STRVAL_P(z), a->name_len)) {
                    enabled_metrics[enabled_metrics_count++] = (struct php_perf_enabled_metric){
                        .avail = a,
                        .id = 0,
                        .fd = -1,
                    };
                }
            }
        }
        ZEND_HASH_FOREACH_END();

        PERF_G(enabled_metrics) = enabled_metrics;
        PERF_G(enabled_metrics_count) = enabled_metrics_count;
    } while (false);

    // Register metrics
    int fd;
    uint64_t id;

    // Register dummy metric to hold the group
    fd = perf_event_open_simple(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_DUMMY, -1);
    if (fd == -1) {
        goto cleanup;
    }
    ioctl(fd, PERF_EVENT_IOC_ID, &id);

    PERF_G(enabled_metrics)[0].fd = fd;
    PERF_G(enabled_metrics)[0].id = id;

    // Register all the other metrics
    for (size_t i = 1; i < PERF_G(enabled_metrics_count); i++) {
        struct php_perf_enabled_metric *m = &PERF_G(enabled_metrics)[i];

        fd = perf_event_open_simple(m->avail->type, m->avail->config, PERF_G(enabled_metrics)[0].fd);
        if (fd == -1) {
            continue;
        }
        ioctl(fd, PERF_EVENT_IOC_ID, &id);

        m->fd = fd;
        m->id = id;
    }

    // Enable
    ioctl(PERF_G(enabled_metrics)[0].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

    return SUCCESS;

cleanup:
    unregister_events();
    return FAILURE;
}

PHP_FUNCTION(perf_stat)
{
    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    if (!PERF_G(enable) || !PERF_G(enabled_metrics)) {
        return;
    }

    ioctl(PERF_G(enabled_metrics)[0].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

    size_t size =
        sizeof(struct read_format) + sizeof(((struct read_format){0}).values[0]) * PERF_G(enabled_metrics_count);
    struct read_format *data = alloca(size);

    ssize_t n = read(PERF_G(enabled_metrics)[0].fd, (void *) data, size);

    if (n == -1) {
        php_error_docref(NULL, E_WARNING, "perf: failed to read: %s", strerror(errno));
        goto done;
    }

    for (size_t i = 0; i < data->nr; i++) {
        for (size_t j = 0; j < PERF_G(enabled_metrics_count); j++) {
            struct php_perf_enabled_metric *e = &PERF_G(enabled_metrics)[j];
            if (e->id == data->values[i].id && e->avail) {
                if (data->values[i].value > (uint64_t) ZEND_LONG_MAX) {
                    php_error_docref(
                        NULL, E_WARNING, "perf: counter truncation for %.*s", (int) e->avail->name_len, e->avail->name
                    );
                    continue;
                }

                add_assoc_long_ex(return_value, e->avail->name, e->avail->name_len, data->values[i].value);
            }
        }
    }

done:
    ioctl(PERF_G(enabled_metrics)[0].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

static PHP_RINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(perf)
{
    return SUCCESS;
}

static PHP_MINIT_FUNCTION(perf)
{
    int flags = CONST_CS | CONST_PERSISTENT;

    // Initialize pfm
    int pfm_ret = pfm_initialize();
    if (pfm_ret != PFM_SUCCESS) {
        php_error_docref(NULL, E_WARNING, "perf: cannot initialize pfm: %s", pfm_strerror(pfm_ret));
        return FAILURE;
    }

    REGISTER_INI_ENTRIES();

    PERF_G(enable) = INI_BOOL("perf.enable");
    PERF_G(metrics) = INI_STR("perf.metrics");

    // Register constants
    REGISTER_STRING_CONSTANT("PerfExt\\VERSION", (char *) PHP_PERF_VERSION, flags);

    if (SUCCESS != php_perf_handle_minit()) {
        return FAILURE;
    }

    if (SUCCESS != php_perf_pmu_enum_minit()) {
        return FAILURE;
    }

    //    for (size_t i = 0; i < available_metrics_count; i++) {
    //        zend_register_long_constant(
    //            available_metrics[i].name, available_metrics[i].name_len, available_metrics[i].flag, flags,
    //            module_number
    //        );
    //    }

    if (register_events() == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(perf)
{
    unregister_events();

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(perf)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERF_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERF_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERF_AUTHORS);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

static PHP_GINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(perf_globals, 0, sizeof(zend_perf_globals));
}

PERF_LOCAL extern ZEND_FUNCTION(perf_list_pmus);
PERF_LOCAL extern ZEND_FUNCTION(perf_list_pmu_events);
PERF_LOCAL extern ZEND_FUNCTION(perf_open);

// clang-format off
const zend_function_entry perf_functions[] = {
    ZEND_RAW_FENTRY("PerfExt\\perf_stat", ZEND_FN(perf_stat), perf_stat_arginfo, 0)
    ZEND_RAW_FENTRY("PerfExt\\list_pmus", ZEND_FN(perf_list_pmus), perf_list_pmus_arginfo, 0)
    ZEND_RAW_FENTRY("PerfExt\\list_pmu_events", ZEND_FN(perf_list_pmu_events), perf_list_pmu_events_arginfo, 0)
    ZEND_RAW_FENTRY("PerfExt\\open", ZEND_FN(perf_open), perf_open_arginfo, 0)
    PHP_FE_END
};
// clang-format on

static const zend_module_dep perf_deps[] = {
    {"opcache", NULL, NULL, MODULE_DEP_OPTIONAL},
    ZEND_MOD_END,
};

zend_module_entry perf_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    perf_deps,                /* Deps */
    PHP_PERF_NAME,            /* Name */
    perf_functions,           /* Functions */
    PHP_MINIT(perf),          /* MINIT */
    PHP_MSHUTDOWN(perf),      /* MSHUTDOWN */
    PHP_RINIT(perf),          /* RINIT */
    PHP_RSHUTDOWN(perf),      /* RSHUTDOWN */
    PHP_MINFO(perf),          /* MINFO */
    PHP_PERF_VERSION,         /* Version */
    PHP_MODULE_GLOBALS(perf), /* Globals */
    PHP_GINIT(perf),          /* GINIT */
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_PERF
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_DLEXPORT zend_module_entry *get_module(void);
ZEND_GET_MODULE(perf) // Common for all PHP extensions which are build as shared modules
#endif
