/**
 * Copyright (C) 2024 John Boehr & contributors
 *
 * This file is part of php-perfidious.
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

#include <unistd.h>
#include <sys/capability.h>
#include <perfmon/pfmlib.h>
#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "main/php.h"
#include "php_perfidious.h"
#include "handle.h"
#include "private.h"

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_get_pmu_info_arginfo, false, 1, Perfidious\\PmuInfo, false)
    ZEND_ARG_TYPE_INFO(false, pmu, IS_LONG, false)
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_get_pmu_info)
{
    zend_long pmu;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pmu)
    ZEND_PARSE_PARAMETERS_END();

    if (SUCCESS != perfidious_get_pmu_info(pmu, return_value, false)) {
        RETURN_NULL();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_global_handle_arginfo, false, 0, Perfidious\\Handle, false)
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_global_handle)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (EXPECTED(PERFIDIOUS_G(global_enable) && PERFIDIOUS_G(global_handle))) {
        object_init_ex(return_value, perfidious_handle_ce);

        struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
        obj->no_auto_close = true;
        obj->handle = PERFIDIOUS_G(global_handle);
    } else {
        RETURN_NULL();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_list_pmus_arginfo, IS_ARRAY, false)
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_list_pmus)
{
    zend_long index;
    zval tmp = {0};

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    pfm_for_all_pmus(index)
    {
        if (SUCCESS == perfidious_get_pmu_info(index, &tmp, true)) {
            add_next_index_zval(return_value, &tmp);
        }
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_list_pmu_events_arginfo, IS_ARRAY, false)
    ZEND_ARG_TYPE_INFO(false, pmu, IS_LONG, false)
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_list_pmu_events)
{
    zend_long pmu_id;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pmu_id)
    ZEND_PARSE_PARAMETERS_END();

    pfm_pmu_t pmu = pmu_id;
    pfm_pmu_info_t pmu_info = {0};
    pfm_err_t pfm_err;
    zval tmp = {0};

    pmu_info.size = sizeof(pmu_info);

    pfm_err = pfm_get_pmu_info(pmu, &pmu_info);
    if (pfm_err != PFM_SUCCESS) {
        zend_throw_exception_ex(
            perfidious_pmu_not_found_exception_ce,
            pfm_err,
            "libpfm: cannot get pmu info for %lu: %s",
            (zend_long) pmu,
            pfm_strerror(pfm_err)
        );
        RETURN_NULL();
    }

    array_init(return_value);

    for (int i = pmu_info.first_event; i != -1; i = pfm_get_event_next(i)) {
        if (UNEXPECTED(FAILURE == perfidious_get_pmu_event_info(&pmu_info, i, &tmp))) {
            zval_ptr_dtor(return_value);
            RETURN_NULL();
        }

        add_next_index_zval(return_value, &tmp);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_open_arginfo, false, 1, Perfidious\\Handle, false)
    ZEND_ARG_TYPE_INFO(false, event_names, IS_ARRAY, false)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(false, pid, IS_LONG, true, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(false, cpu, IS_LONG, true, "-1")
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_open)
{
    HashTable *event_names_ht;
    zend_long pid_zl = 0;
    zend_long cpu = -1;
    zval *z;
    pid_t pid;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_ARRAY_HT(event_names_ht);
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pid_zl)
        Z_PARAM_LONG(cpu)
    ZEND_PARSE_PARAMETERS_END();

    if (false == perfidious_zend_long_to_pid_t(pid_zl, &pid)) {
        RETURN_NULL();
    }

    // Check capability if pid > 0
#ifndef PERFIDIOUS_DEBUG
    if (pid > 0) {
        cap_t cap = cap_get_proc();
        if (cap != NULL) {
            cap_flag_value_t v = CAP_CLEAR;
            cap_get_flag(cap, CAP_PERFMON, CAP_EFFECTIVE, &v);
            if (v == CAP_CLEAR) {
                zend_throw_exception_ex(perfidious_io_exception_ce, 0, "pid greater than zero and CAP_PERFMON not set");
                return;
            }
        }
    }
#endif

    // Check cpu for overflow
    long int n_proc_onln = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu > n_proc_onln) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "cpu too large: %ld > %ld", cpu, n_proc_onln);
        return;
    }

    // Copy event names into an array
    zend_string **arr = alloca(sizeof(zend_string *) * (zend_array_count(event_names_ht) + 1));
    size_t arr_count = 0;

    ZEND_HASH_FOREACH_VAL(event_names_ht, z)
    {
        if (EXPECTED(Z_TYPE_P(z) == IS_STRING)) {
            arr[arr_count++] = Z_STR_P(z);
        } else {
            zend_type_error("All event names must be strings");
        }
    }
    ZEND_HASH_FOREACH_END();

    arr[arr_count] = NULL;

    struct perfidious_handle *handle = perfidious_handle_open_ex(arr, arr_count, pid, (int) cpu, false);

    if (UNEXPECTED(NULL == handle)) {
        RETURN_NULL();
    }

    object_init_ex(return_value, perfidious_handle_ce);

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
    obj->handle = handle;
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_request_handle_arginfo, false, 0, Perfidious\\Handle, true)
ZEND_END_ARG_INFO()

ZEND_COLD
static PHP_FUNCTION(perfidious_request_handle)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (EXPECTED(PERFIDIOUS_G(request_enable) && PERFIDIOUS_G(request_handle))) {
        object_init_ex(return_value, perfidious_handle_ce);

        struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
        obj->no_auto_close = true;
        obj->handle = PERFIDIOUS_G(request_handle);
    } else {
        RETURN_NULL();
    }
}

#ifdef PERFIDIOUS_DEBUG
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(perfidious_debug_uint64_overflow_arginfo, false, 0, IS_LONG, false)
    ZEND_ARG_TYPE_INFO(false, overflow_mode, IS_LONG, false)
ZEND_END_ARG_INFO()

ZEND_COLD
static ZEND_FUNCTION(perfidious_debug_uint64_overflow)
{
    zend_long overflow_mode_zl = -1;
    enum perfidious_overflow_mode overflow_mode;
    zend_long output;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(overflow_mode_zl)
    ZEND_PARSE_PARAMETERS_END();

    if (overflow_mode_zl == -1) {
        overflow_mode = PERFIDIOUS_G(overflow_mode);
    } else {
        if (overflow_mode_zl > PERFIDIOUS_OVERFLOW_MAX || overflow_mode_zl < 0) {
            zend_type_error("Overflow mode out-of-range");
            return;
        }
        overflow_mode = overflow_mode_zl;
    }

    if (!perfidious_uint64_t_to_zend_long(UINT64_MAX, &output, overflow_mode)) {
        RETURN_NULL();
    }

    RETURN_LONG(output);
}
#endif

// clang-format off
PERFIDIOUS_LOCAL
const zend_function_entry perfidious_functions[] = {
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\get_pmu_info", ZEND_FN(perfidious_get_pmu_info), perfidious_get_pmu_info_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\global_handle", ZEND_FN(perfidious_global_handle), perfidious_global_handle_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\list_pmus", ZEND_FN(perfidious_list_pmus), perfidious_list_pmus_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\list_pmu_events", ZEND_FN(perfidious_list_pmu_events), perfidious_list_pmu_events_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\open", ZEND_FN(perfidious_open), perfidious_open_arginfo, 0)
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\request_handle", ZEND_FN(perfidious_request_handle), perfidious_request_handle_arginfo, 0)
#ifdef PERFIDIOUS_DEBUG
    ZEND_RAW_FENTRY(PHP_PERFIDIOUS_NAMESPACE "\\debug_uint64_overflow", ZEND_FN(perfidious_debug_uint64_overflow), perfidious_debug_uint64_overflow_arginfo, 0)
#endif
    PHP_FE_END
};
// clang-format on
