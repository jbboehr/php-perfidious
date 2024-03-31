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

#include <perfmon/pfmlib.h>
#include "Zend/zend_API.h"
#include "Zend/zend_enum.h"
#include "main/php.h"
#include "php_perf.h"
#include "./handle.h"

PERF_LOCAL
PHP_FUNCTION(perf_list_pmus)
{
    unsigned long index = 0;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    pfm_for_all_pmus(index)
    {
        pfm_pmu_info_t pinfo = {0};
        int ret;
        zval tmp = {0};

        pinfo.size = sizeof(pinfo);

        ret = pfm_get_pmu_info(index, &pinfo);
        if (ret != PFM_SUCCESS) {
            // php_error_docref(NULL, E_WARNING, "perf: libpfm: cannot get pmu info for %lu: %s", index,
            // pfm_strerror(ret));
            continue;
        }

        array_init(&tmp);

        object_init_ex(&tmp, perf_pmu_info_ce);

        zend_update_property_string(Z_OBJCE(tmp), Z_OBJ(tmp), "name", sizeof("name") - 1, pinfo.name);
        zend_update_property_string(Z_OBJCE(tmp), Z_OBJ(tmp), "desc", sizeof("desc") - 1, pinfo.desc);
        zend_update_property_long(Z_OBJCE(tmp), Z_OBJ(tmp), "pmu", sizeof("pmu") - 1, (zend_long) pinfo.pmu);
        zend_update_property_long(Z_OBJCE(tmp), Z_OBJ(tmp), "type", sizeof("type") - 1, (zend_long) pinfo.type);
        zend_update_property_long(
            Z_OBJCE(tmp), Z_OBJ(tmp), "nevents", sizeof("nevents") - 1, (zend_long) pinfo.nevents
        );

        add_next_index_zval(return_value, &tmp);

        ZVAL_UNDEF(&tmp);
    }
}

PERF_LOCAL
PHP_FUNCTION(perf_list_pmu_events)
{
    zend_long pmu_id;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pmu_id)
    ZEND_PARSE_PARAMETERS_END();

    pfm_pmu_t pmu = pmu_id;
    pfm_event_info_t info = {0};
    pfm_pmu_info_t pinfo = {0};
    int i;
    int ret;

    array_init(return_value);

    info.size = sizeof(info);
    pinfo.size = sizeof(pinfo);

    ret = pfm_get_pmu_info(pmu, &pinfo);
    if (ret != PFM_SUCCESS) {
        php_error_docref(NULL, E_WARNING, "perf: libpfm: cannot get pmu info");
        return;
    }

    for (i = pinfo.first_event; i != -1; i = pfm_get_event_next(i)) {
        ret = pfm_get_event_info(i, PFM_OS_PERF_EVENT, &info);
        if (ret != PFM_SUCCESS) {
            php_error_docref(NULL, E_WARNING, "perf: libpfm: cannot get event info");
            return;
        }

        char buf[512];
        zval tmp = {0};
        array_init(&tmp);

        size_t buf_len = snprintf(buf, sizeof(buf), "%s::%s", pinfo.name, info.name);

        object_init_ex(&tmp, perf_pmu_event_info_ce);

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

PERF_LOCAL
PHP_FUNCTION(perf_open)
{
    HashTable *event_names_ht;
    zval *z;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY_HT(event_names_ht);
    ZEND_PARSE_PARAMETERS_END();

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

    struct php_perf_handle *handle = php_perf_handle_open(arr, arr_count, 0);

    object_init_ex(return_value, perf_handle_ce);

    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(Z_OBJ_P(return_value));
    obj->handle = handle;

    // if (Z_OBJCE_P(return_value)->constructor) {
    //     zend_call_known_instance_method_with_0_params(
    //         Z_OBJCE_P(return_value)->constructor, Z_OBJ_P(return_value), NULL
    //     );
    // }

    // if (EG(exception)) {
    //     php_perf_handle_close(handle);
    //     RETURN_NULL();
    // }
}
