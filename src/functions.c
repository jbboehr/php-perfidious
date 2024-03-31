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
            continue;
            //            php_error_docref(NULL, E_WARNING, "perf: libpfm: cannot get pmu info for %lu: %s", index,
            //            pfm_strerror(ret)); return;
        }

        array_init(&tmp);

        add_assoc_string_ex(&tmp, ZEND_STRL("name"), pinfo.name);
        add_assoc_string_ex(&tmp, ZEND_STRL("desc"), pinfo.desc);
        add_assoc_long_ex(&tmp, ZEND_STRL("pmu"), (zend_long) pinfo.pmu);
        add_assoc_long_ex(&tmp, ZEND_STRL("type"), (zend_long) pinfo.type);
        add_assoc_long_ex(&tmp, ZEND_STRL("nevents"), (zend_long) pinfo.nevents);

        add_next_index_zval(return_value, &tmp);
    }
}

PERF_LOCAL
PHP_FUNCTION(perf_list_pmu_events)
{
    zend_object *pmu_enum = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJ_OF_CLASS(pmu_enum, perf_pmu_enum_ce)
    ZEND_PARSE_PARAMETERS_END();

    zval *pmu_z = zend_enum_fetch_case_value(pmu_enum);
    ZEND_ASSERT(EXPECTED(pmu_z != NULL && Z_TYPE_P(pmu_z) == IS_LONG));

    pfm_pmu_t pmu = Z_LVAL_P(pmu_z);
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
        add_assoc_stringl_ex(&tmp, ZEND_STRL("name"), buf, buf_len);

        add_assoc_bool_ex(&tmp, ZEND_STRL("is_present"), pinfo.is_present);

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

    const char **arr = ecalloc(sizeof(const char *), zend_array_count(event_names_ht) + 1);
    size_t arr_count = 0;

    ZEND_HASH_FOREACH_VAL(event_names_ht, z)
    {
        if (Z_TYPE_P(z) == IS_STRING) {
            arr[arr_count++] = Z_STRVAL_P(z);
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
