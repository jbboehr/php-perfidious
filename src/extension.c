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

#define DEFAULT_METRICS "perf::PERF_FLAG_HW_CPU_CYCLES,perf::PERF_FLAG_HW_INSTRUCTIONS"

ZEND_DECLARE_MODULE_GLOBALS(perf);

PERF_LOCAL zend_result php_perf_handle_minit(void);
PERF_LOCAL zend_result php_perf_pmu_enum_minit(void);

// clang-format off
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("perf.global.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, global_enable, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.global.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateString, global_metrics, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.request.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, request_enable, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.request.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateString, request_metrics, zend_perf_globals, perf_globals)
PHP_INI_END()
// clang-format on

static PHP_RINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (PERF_G(request_handle)) {
        php_perf_handle_reset(PERF_G(request_handle));
        php_perf_handle_enable(PERF_G(request_handle));
    }

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(perf)
{
    if (PERF_G(request_handle)) {
        php_perf_handle_reset(PERF_G(request_handle));
        php_perf_handle_disable(PERF_G(request_handle));
    }

    return SUCCESS;
}

static struct php_perf_handle *split_and_open(const char *metrics, bool persist)
{
    zval z_metrics = {0};
    struct php_perf_handle *handle;

    zend_string *delim = zend_string_init_fast(ZEND_STRL(","));
    zend_string *orig = zend_string_init(metrics, strlen(metrics), persist);
    array_init(&z_metrics);
    php_explode(delim, orig, &z_metrics, ZEND_LONG_MAX);
    zend_string_release(delim);
    zend_string_release(orig);

    if (Z_TYPE(z_metrics) != IS_ARRAY) {
        zval_dtor(&z_metrics);
        return NULL;
    }

    zend_string **arr = pecalloc(zend_array_count(Z_ARRVAL(z_metrics)), sizeof(zend_string *), persist);
    size_t arr_len = 0;

    zval *z;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL(z_metrics), z)
    {
        if (Z_TYPE_P(z) != IS_STRING) {
            continue;
        }

        arr[arr_len++] = Z_STR_P(z);
    }
    ZEND_HASH_FOREACH_END();

    handle = php_perf_handle_open(arr, arr_len, persist);

    zval_dtor(&z_metrics);
    pefree(arr, persist);

    return handle;
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

    REGISTER_STRING_CONSTANT("PerfExt\\VERSION", (char *) PHP_PERF_VERSION, flags);

    if (SUCCESS != php_perf_handle_minit()) {
        return FAILURE;
    }

    if (SUCCESS != php_perf_pmu_enum_minit()) {
        return FAILURE;
    }

    if (PERF_G(global_enable) && PERF_G(global_metrics) != NULL) {
        struct php_perf_handle *handle = NULL;
        if (PERF_G(global_metrics) != NULL) {
            handle = split_and_open(PERF_G(global_metrics), true);
        }
        PERF_G(global_handle) = handle;
        if (handle == NULL) {
            PERF_G(global_enable) = false;
        }
    }

    if (PERF_G(request_enable)) {
        struct php_perf_handle *handle = NULL;
        if (PERF_G(request_metrics) != NULL) {
            handle = split_and_open(PERF_G(request_metrics), true);
        }
        PERF_G(request_handle) = handle;
        if (handle == NULL) {
            PERF_G(request_enable) = false;
        }
    }

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(perf)
{
    if (PERF_G(request_handle)) {
        php_perf_handle_close(PERF_G(request_handle));
        PERF_G(request_handle) = NULL;
    }

    if (PERF_G(global_handle)) {
        php_perf_handle_close(PERF_G(global_handle));
        PERF_G(global_handle) = NULL;
    }

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static void minfo_handle_metrics(struct php_perf_handle *handle)
{
    zval z_metrics = {0};

    php_perf_handle_read_to_array(handle, &z_metrics);

    zend_string *key;
    zval *val;
    char buf[512];

    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL(z_metrics), key, val)
    {
        if (key != NULL && Z_TYPE_P(val) == IS_LONG) {
            snprintf(buf, sizeof(buf), "%lu", Z_LVAL_P(val));
            php_info_print_table_row(2, ZSTR_VAL(key), buf);
        }
    }
    ZEND_HASH_FOREACH_END();

    zval_ptr_dtor(&z_metrics);
}

static PHP_MINFO_FUNCTION(perf)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERF_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERF_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERF_AUTHORS);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();

    if (PERF_G(global_enable) && PERF_G(global_handle) != NULL) {
        php_info_print_table_start();
        php_info_print_table_colspan_header(2, "Global Metrics");
        minfo_handle_metrics(PERF_G(global_handle));
        php_info_print_table_end();
    }

    if (PERF_G(request_enable) && PERF_G(request_handle) != NULL) {
        php_info_print_table_start();
        php_info_print_table_colspan_header(2, "Request Metrics");
        minfo_handle_metrics(PERF_G(request_handle));
        php_info_print_table_end();
    }
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
