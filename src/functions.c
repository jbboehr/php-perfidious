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

#include <unistd.h>
#include <sys/capability.h>
#include <perfmon/pfmlib.h>
#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "main/php.h"
#include "php_perf.h"
#include "./functions.h"
#include "./handle.h"

ZEND_COLD
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_result perfidious_get_pmu_info(zend_long pmu, zval *rv, bool silent)
{
    pfm_pmu_info_t pmu_info = {0};
    int ret;

    pmu_info.size = sizeof(pmu_info);

    ret = pfm_get_pmu_info(pmu, &pmu_info);
    if (ret != PFM_SUCCESS) {
        if (!silent) {
            zend_throw_exception_ex(
                perfidious_pmu_not_found_exception_ce, ret, "cannot get pmu info for %lu: %s", pmu, pfm_strerror(ret)
            );
        }
        return FAILURE;
    }

    ZVAL_UNDEF(rv);
    object_init_ex(rv, perfidious_pmu_info_ce);

    zend_update_property_string(Z_OBJCE_P(rv), Z_OBJ_P(rv), "name", sizeof("name") - 1, pmu_info.name);
    zend_update_property_string(Z_OBJCE_P(rv), Z_OBJ_P(rv), "desc", sizeof("desc") - 1, pmu_info.desc);
    zend_update_property_long(Z_OBJCE_P(rv), Z_OBJ_P(rv), "pmu", sizeof("pmu") - 1, (zend_long) pmu_info.pmu);
    zend_update_property_long(Z_OBJCE_P(rv), Z_OBJ_P(rv), "type", sizeof("type") - 1, (zend_long) pmu_info.type);
    zend_update_property_long(
        Z_OBJCE_P(rv), Z_OBJ_P(rv), "nevents", sizeof("nevents") - 1, (zend_long) pmu_info.nevents
    );
    zend_update_property_bool(Z_OBJCE_P(rv), Z_OBJ_P(rv), "is_present", sizeof("is_present") - 1, pmu_info.is_present);

    return SUCCESS;
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_get_pmu_info)
{
    zend_long pmu;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pmu)
    ZEND_PARSE_PARAMETERS_END();

    if (SUCCESS != perfidious_get_pmu_info(pmu, return_value, false)) {
        RETURN_NULL();
    }
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_global_handle)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (PERF_G(global_enable) && PERF_G(global_handle)) {
        object_init_ex(return_value, perfidious_handle_ce);

        struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
        obj->no_auto_close = true;
        obj->handle = PERF_G(global_handle);
    } else {
        RETURN_NULL();
    }
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_list_pmus)
{
    unsigned long index = 0;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    pfm_for_all_pmus(index)
    {
        zval tmp = {0};
        zend_result result = perfidious_get_pmu_info((zend_long) index, &tmp, true);
        if (result == SUCCESS) {
            add_next_index_zval(return_value, &tmp);
        }
        ZVAL_UNDEF(&tmp);
    }
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_list_pmu_events)
{
    zend_long pmu_id;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pmu_id)
    ZEND_PARSE_PARAMETERS_END();

    pfm_pmu_t pmu = pmu_id;
    pfm_event_info_t info = {0};
    pfm_pmu_info_t pinfo = {0};
    int ret;

    array_init(return_value);

    info.size = sizeof(info);
    pinfo.size = sizeof(pinfo);

    ret = pfm_get_pmu_info(pmu, &pinfo);
    if (ret != PFM_SUCCESS) {
        zend_throw_exception_ex(
            perfidious_pmu_not_found_exception_ce,
            ret,
            "libpfm: cannot get pmu info for %lu: %s",
            (zend_long) pmu,
            pfm_strerror(ret)
        );
        return;
    }

    for (int i = pinfo.first_event; i != -1; i = pfm_get_event_next(i)) {
        ret = pfm_get_event_info(i, PFM_OS_PERF_EVENT, &info);
        if (ret != PFM_SUCCESS) {
            ZVAL_UNDEF(return_value);
            zend_throw_exception_ex(
                perfidious_pmu_event_not_found_exception_ce,
                ret,
                "libpfm: cannot get event info for %lu: %s",
                (zend_long) i,
                pfm_strerror(ret)
            );
            return;
        }

        char buf[512];
        zval tmp = {0};
        array_init(&tmp);

        size_t buf_len = snprintf(buf, sizeof(buf), "%s::%s", pinfo.name, info.name);

        object_init_ex(&tmp, perfidious_pmu_event_info_ce);

        zend_update_property_stringl(Z_OBJCE(tmp), Z_OBJ(tmp), "name", sizeof("name") - 1, buf, buf_len);
        zend_update_property_string(Z_OBJCE(tmp), Z_OBJ(tmp), "desc", sizeof("desc") - 1, info.desc);
        if (info.equiv) {
            zend_update_property_string(Z_OBJCE(tmp), Z_OBJ(tmp), "equiv", sizeof("equiv") - 1, info.equiv);
        } else {
            zend_update_property_null(Z_OBJCE(tmp), Z_OBJ(tmp), "equiv", sizeof("equiv") - 1);
        }
        zend_update_property_long(Z_OBJCE(tmp), Z_OBJ(tmp), "pmu", sizeof("pmu") - 1, (zend_long) info.pmu);
        zend_update_property_bool(Z_OBJCE(tmp), Z_OBJ(tmp), "is_present", sizeof("is_present") - 1, pinfo.is_present);

        add_next_index_zval(return_value, &tmp);
    }
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_open)
{
    HashTable *event_names_ht;
    zend_long pid = 0;
    zend_long cpu = -1;
    zval *z;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_ARRAY_HT(event_names_ht);
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pid)
        Z_PARAM_LONG(cpu)
    ZEND_PARSE_PARAMETERS_END();

#if SIZEOF_ZEND_LONG > SIZEOF_PID_T
    // Check pid for overflow
    const zend_long PID_MAX = (((zend_long) 1) << ((SIZEOF_PID_T * 8) - 1)) - 1;
    if (pid > PID_MAX) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "pid too large: %ld > %ld", pid, PID_MAX);
        return;
    }
#endif

    // Check capability if pid > 0
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

    // Check cpu for overflow
    long int n_proc_onln = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu > n_proc_onln) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "cpu too large: %ld > %ld", cpu, n_proc_onln);
        return;
    }

    // Copy event names into an array
    zend_string **arr = ecalloc(sizeof(zend_string *), zend_array_count(event_names_ht) + 1);
    size_t arr_count = 0;

    ZEND_HASH_FOREACH_VAL(event_names_ht, z)
    {
        if (Z_TYPE_P(z) == IS_STRING) {
            arr[arr_count++] = Z_STR_P(z);
        }
    }
    ZEND_HASH_FOREACH_END();

    arr[arr_count] = NULL;

    struct perfidious_handle *handle = perfidious_handle_open_ex(arr, arr_count, (pid_t) pid, (int) cpu, false);

    object_init_ex(return_value, perfidious_handle_ce);

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
    obj->handle = handle;
}

ZEND_COLD
PERFIDIOUS_LOCAL
PHP_FUNCTION(perfidious_request_handle)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (PERF_G(request_enable) && PERF_G(request_handle)) {
        object_init_ex(return_value, perfidious_handle_ce);

        struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(return_value));
        obj->no_auto_close = true;
        obj->handle = PERF_G(request_handle);
    } else {
        RETURN_NULL();
    }
}
