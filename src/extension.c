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

#include <inttypes.h>
#include <perfmon/pfmlib.h>

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_ini.h"
#include "Zend/zend_modules.h"
#include "Zend/zend_operators.h"
#include "main/php.h"
#include "main/php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "php_perf.h"
#include "./functions.h"

#define DEFAULT_METRICS "perf::PERF_FLAG_HW_CPU_CYCLES,perf::PERF_FLAG_HW_INSTRUCTIONS"

ZEND_DECLARE_MODULE_GLOBALS(perf);

PERFIDIOUS_LOCAL zend_result perfidious_exceptions_minit(void);
PERFIDIOUS_LOCAL zend_result perfidious_handle_minit(void);
PERFIDIOUS_LOCAL zend_result perfidious_pmu_event_info_minit(void);
PERFIDIOUS_LOCAL zend_result perfidious_pmu_info_minit(void);

#if PHP_VERSION_ID < 80200
static ZEND_INI_MH(OnUpdateStr)
{
    zend_string **p = (zend_string **) (void *) ZEND_INI_GET_ADDR();
    *p = new_value;
    return SUCCESS;
}
#endif

// clang-format off
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("perf.global.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, global_enable, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.global.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateStr, global_metrics, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.request.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, request_enable, zend_perf_globals, perf_globals)
    STD_PHP_INI_ENTRY("perf.request.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateStr, request_metrics, zend_perf_globals, perf_globals)
PHP_INI_END()
// clang-format on

static PHP_RINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (PERF_G(request_handle)) {
        perfidious_handle_reset(PERF_G(request_handle));
        perfidious_handle_enable(PERF_G(request_handle));
    }

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(perf)
{
    if (PERF_G(request_handle)) {
        perfidious_handle_reset(PERF_G(request_handle));
        perfidious_handle_disable(PERF_G(request_handle));
    }

    return SUCCESS;
}

static struct perfidious_handle *split_and_open(zend_string *metrics, bool persist)
{
    zval z_metrics = {0};
    struct perfidious_handle *handle;

    do {
        zend_string *delim = zend_string_init_fast(ZEND_STRL(","));
        array_init(&z_metrics);
        php_explode(delim, metrics, &z_metrics, ZEND_LONG_MAX);
        zend_string_release(delim);
    } while (false);

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

    handle = perfidious_handle_open(arr, arr_len, persist);

    if (handle != NULL) {
        perfidious_handle_enable(handle);
    }

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

    REGISTER_STRING_CONSTANT(PHP_PERF_NAMESPACE "\\VERSION", (char *) PHP_PERF_VERSION, flags);

    if (SUCCESS != perfidious_exceptions_minit()) {
        return FAILURE;
    }

    if (SUCCESS != perfidious_handle_minit()) {
        return FAILURE;
    }

    if (SUCCESS != perfidious_pmu_event_info_minit()) {
        return FAILURE;
    }

    if (SUCCESS != perfidious_pmu_info_minit()) {
        return FAILURE;
    }

    if (PERF_G(global_enable) && PERF_G(global_metrics) != NULL) {
        struct perfidious_handle *handle = NULL;
        if (PERF_G(global_metrics) != NULL) {
            handle = split_and_open(PERF_G(global_metrics), true);
        }
        PERF_G(global_handle) = handle;
        if (handle == NULL) {
            PERF_G(global_enable) = false;
        }
    }

    if (PERF_G(request_enable)) {
        struct perfidious_handle *handle = NULL;
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
        perfidious_handle_close(PERF_G(request_handle));
        PERF_G(request_handle) = NULL;
    }

    if (PERF_G(global_handle)) {
        perfidious_handle_close(PERF_G(global_handle));
        PERF_G(global_handle) = NULL;
    }

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static void minfo_handle_metrics(struct perfidious_handle *handle)
{
    zval z_metrics = {0};

    perfidious_handle_read_to_array(handle, &z_metrics);

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

// clang-format off
const zend_function_entry perf_functions[] = {
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\get_pmu_info", ZEND_FN(perfidious_get_pmu_info), perfidious_get_pmu_info_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\global_handle", ZEND_FN(perfidious_global_handle), perfidious_global_handle_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\list_pmus", ZEND_FN(perfidious_list_pmus), perfidious_list_pmus_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\list_pmu_events", ZEND_FN(perfidious_list_pmu_events), perfidious_list_pmu_events_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\open", ZEND_FN(perfidious_open), perfidious_open_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERF_NAMESPACE "\\request_handle", ZEND_FN(perfidious_request_handle), perfidious_request_handle_arginfo, 0)
    PHP_FE_END
};
// clang-format on

static const zend_module_dep perf_deps[] = {
    {"spl",     NULL, NULL, MODULE_DEP_REQUIRED},
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
