/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
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

#include "php_perfidious.h"
#include "handle.h"
#include "private.h"

#define DEFAULT_METRICS "perf::PERF_COUNT_HW_CPU_CYCLES:u,perf::PERF_COUNT_HW_INSTRUCTIONS:u"

ZEND_DECLARE_MODULE_GLOBALS(perfidious);

PERFIDIOUS_LOCAL void perfidious_exceptions_minit(void);
PERFIDIOUS_LOCAL void perfidious_handle_minit(void);
PERFIDIOUS_LOCAL void perfidious_pmu_event_info_minit(void);
PERFIDIOUS_LOCAL void perfidious_pmu_info_minit(void);
PERFIDIOUS_LOCAL void perfidious_read_result_minit(void);

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
    STD_PHP_INI_ENTRY(PHP_PERFIDIOUS_NAME ".overflow_mode", "0", PHP_INI_SYSTEM, OnUpdateLong, overflow_mode, zend_perfidious_globals, perfidious_globals)
    STD_PHP_INI_ENTRY(PHP_PERFIDIOUS_NAME ".global.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, global_enable, zend_perfidious_globals, perfidious_globals)
    STD_PHP_INI_ENTRY(PHP_PERFIDIOUS_NAME ".global.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateStr, global_metrics, zend_perfidious_globals, perfidious_globals)
    STD_PHP_INI_ENTRY(PHP_PERFIDIOUS_NAME ".request.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, request_enable, zend_perfidious_globals, perfidious_globals)
    STD_PHP_INI_ENTRY(PHP_PERFIDIOUS_NAME ".request.metrics", DEFAULT_METRICS, PHP_INI_SYSTEM, OnUpdateStr, request_metrics, zend_perfidious_globals, perfidious_globals)
PHP_INI_END()
// clang-format on

static PHP_RINIT_FUNCTION(perfidious)
{
#if defined(COMPILE_DL_PERFIDIOUS) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (PERFIDIOUS_G(request_handle)) {
        perfidious_handle_reset(PERFIDIOUS_G(request_handle));
        perfidious_handle_enable(PERFIDIOUS_G(request_handle));
    }

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(perfidious)
{
    if (PERFIDIOUS_G(request_handle)) {
        perfidious_handle_reset(PERFIDIOUS_G(request_handle));
        perfidious_handle_disable(PERFIDIOUS_G(request_handle));
    }

    return SUCCESS;
}

static struct perfidious_handle *split_and_open(zend_string *restrict metrics, bool persist)
{
    zval z_metrics = {0};
    struct perfidious_handle *handle = NULL;

    do {
        zend_string *delim = zend_string_init_fast(ZEND_STRL(","));
        array_init(&z_metrics);
        php_explode(delim, metrics, &z_metrics, ZEND_LONG_MAX);
        zend_string_release(delim);
    } while (false);

    ZEND_ASSERT(Z_TYPE(z_metrics) == IS_ARRAY);

    do {
        zend_string **arr = alloca(sizeof(zend_string *) * (zend_array_count(Z_ARRVAL(z_metrics)) + 1));
        size_t arr_len = 0;
        zval *z;

        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(z_metrics), z)
        {
            ZEND_ASSERT(Z_TYPE_P(z) == IS_STRING);
            arr[arr_len++] = Z_STR_P(z);
        }
        ZEND_HASH_FOREACH_END();

        arr[arr_len] = NULL;
        handle = perfidious_handle_open(arr, arr_len, persist);
    } while (false);

    if (EXPECTED(handle != NULL)) {
        perfidious_handle_enable(handle);
    }

    zval_dtor(&z_metrics);
    return handle;
}

const char *PERFIDIOUS_MOTD =
    "Think not that I am come to send peace on earth: I came not to send peace, but a sword. Matthew 10:34";

static PHP_MINIT_FUNCTION(perfidious)
{
    const int flags = CONST_CS | CONST_PERSISTENT;

    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_WARNING;

    // Initialize pfm
    int pfm_ret = pfm_initialize();
    if (pfm_ret != PFM_SUCCESS) {
        php_error_docref(NULL, E_WARNING, PHP_PERFIDIOUS_NAME ": cannot initialize libpfm: %s", pfm_strerror(pfm_ret));
        return FAILURE;
    }

#ifdef PERFIDIOUS_DEBUG
    REGISTER_BOOL_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\DEBUG", (zend_bool) PERFIDIOUS_DEBUG, flags);
#else
    REGISTER_BOOL_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\DEBUG", false, flags);
#endif
    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\VERSION", (char *) PHP_PERFIDIOUS_VERSION, flags);

    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_THROW", PERFIDIOUS_OVERFLOW_THROW, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_WARN", PERFIDIOUS_OVERFLOW_WARN, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_SATURATE", PERFIDIOUS_OVERFLOW_SATURATE, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_WRAP", PERFIDIOUS_OVERFLOW_WRAP, flags);

#ifdef PERFIDIOUS_DEBUG
    do {
        char buf[128];
        snprintf(buf, sizeof(buf), "%" PRIu64, UINT64_MAX);
        REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\UINT64_MAX", buf, flags);
    } while (false);
#endif

    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\MOTD", (char *) PERFIDIOUS_MOTD, flags);

    REGISTER_INI_ENTRIES();

    perfidious_exceptions_minit();
    perfidious_handle_minit();
    perfidious_pmu_event_info_minit();
    perfidious_pmu_info_minit();
    perfidious_read_result_minit();

    if (PERFIDIOUS_G(global_enable)) {
        struct perfidious_handle *handle = NULL;
        if (EXPECTED(PERFIDIOUS_G(global_metrics) != NULL)) {
            handle = split_and_open(PERFIDIOUS_G(global_metrics), true);
        }
        PERFIDIOUS_G(global_handle) = handle;
        if (UNEXPECTED(handle == NULL)) {
            PERFIDIOUS_G(global_enable) = false;
        }
    }

    if (PERFIDIOUS_G(request_enable)) {
        struct perfidious_handle *handle = NULL;
        if (EXPECTED(PERFIDIOUS_G(request_metrics) != NULL)) {
            handle = split_and_open(PERFIDIOUS_G(request_metrics), true);
        }
        PERFIDIOUS_G(request_handle) = handle;
        if (UNEXPECTED(handle == NULL)) {
            PERFIDIOUS_G(request_enable) = false;
        }
    }

    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_THROW;

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(perfidious)
{
    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_WARNING;

    if (PERFIDIOUS_G(request_handle)) {
        perfidious_handle_close(PERFIDIOUS_G(request_handle));
        PERFIDIOUS_G(request_handle) = NULL;
    }

    if (PERFIDIOUS_G(global_handle)) {
        perfidious_handle_close(PERFIDIOUS_G(global_handle));
        PERFIDIOUS_G(global_handle) = NULL;
    }

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static zend_always_inline void minfo_handle_metrics(struct perfidious_handle *restrict handle)
{
    zval z_metrics = {0};
    uint64_t time_enabled;
    uint64_t time_running;

    if (UNEXPECTED(
            FAILURE == perfidious_handle_read_to_array_with_times(handle, &z_metrics, &time_enabled, &time_running)
        )) {
        php_info_print_table_colspan_header(3, "READ ERROR");
        return;
    }

    uint64_t perc_running = 0;
    if (time_enabled > 0) {
        perc_running = 100 * time_running / time_enabled;
    }

    zend_string *key;
    zval *val;
    char buf[128];
    char buf2[128];
    char buf3[128];

    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL(z_metrics), key, val)
    {
        if (EXPECTED(key != NULL && Z_TYPE_P(val) == IS_LONG)) {
            uint64_t scaled = time_running > 0 ? Z_LVAL_P(val) * time_enabled / time_running : 0;

            snprintf(buf, sizeof(buf), "%lu", Z_LVAL_P(val));
            snprintf(buf2, sizeof(buf2), "%" PRIu64, scaled);
            snprintf(buf3, sizeof(buf3), "%" PRIu64 "%%", perc_running);
            php_info_print_table_row(4, ZSTR_VAL(key), buf, buf2, buf3);
        }
    }
    ZEND_HASH_FOREACH_END();

    zval_ptr_dtor(&z_metrics);
}

static PHP_MINFO_FUNCTION(perfidious)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERFIDIOUS_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERFIDIOUS_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERFIDIOUS_AUTHORS);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();

    if (PERFIDIOUS_G(global_enable) && PERFIDIOUS_G(global_handle) != NULL) {
        php_info_print_table_start();
        php_info_print_table_colspan_header(4, "Global Metrics");
        php_info_print_table_header(4, "Event", "Counter", "Scaled", "% Running");
        minfo_handle_metrics(PERFIDIOUS_G(global_handle));
        php_info_print_table_end();
    }

    if (PERFIDIOUS_G(request_enable) && PERFIDIOUS_G(request_handle) != NULL) {
        php_info_print_table_start();
        php_info_print_table_colspan_header(4, "Request Metrics");
        php_info_print_table_header(4, "Event", "Counter", "Scaled", "% Running");
        minfo_handle_metrics(PERFIDIOUS_G(request_handle));
        php_info_print_table_end();
    }

    php_info_print_box_start(0);
    PUTS(PERFIDIOUS_MOTD);
    php_info_print_box_end();
}

static PHP_GINIT_FUNCTION(perfidious)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(perfidious_globals, 0, sizeof(zend_perfidious_globals));
    perfidious_globals->error_mode = PERFIDIOUS_ERROR_MODE_THROW;
}

static const zend_module_dep perfidious_deps[] = {
    {"spl",     NULL, NULL, MODULE_DEP_REQUIRED},
    {"opcache", NULL, NULL, MODULE_DEP_OPTIONAL},
    ZEND_MOD_END,
};

PERFIDIOUS_LOCAL
extern const zend_function_entry perfidious_functions[];

zend_module_entry perfidious_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    perfidious_deps,                /* Deps */
    PHP_PERFIDIOUS_NAME,            /* Name */
    perfidious_functions,           /* Functions */
    PHP_MINIT(perfidious),          /* MINIT */
    PHP_MSHUTDOWN(perfidious),      /* MSHUTDOWN */
    PHP_RINIT(perfidious),          /* RINIT */
    PHP_RSHUTDOWN(perfidious),      /* RSHUTDOWN */
    PHP_MINFO(perfidious),          /* MINFO */
    PHP_PERFIDIOUS_VERSION,         /* Version */
    PHP_MODULE_GLOBALS(perfidious), /* Globals */
    PHP_GINIT(perfidious),          /* GINIT */
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_PERFIDIOUS
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_DLEXPORT zend_module_entry *get_module(void);
ZEND_GET_MODULE(perfidious) // Common for all PHP extensions which are build as shared modules
#endif
