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

#include <perfmon/pfmlib.h>
#include <Zend/zend_API.h>

#include "php_perfidious.h"
#include "private.h"

PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_EQUIV;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_IDX;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_event_info_ce;

ZEND_COLD
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static inline zend_result
perfidious_pmu_event_info_ctor(pfm_pmu_info_t *pmu_info, pfm_event_info_t *info, zval *restrict return_value)
{
    char buf[512];
    zval tmp = {0};

    PERFIDIOUS_ASSERT_RETURN(SUCCESS == object_init_ex(return_value, perfidious_pmu_event_info_ce));

    size_t buf_len = snprintf(buf, sizeof(buf), "%s::%s", pmu_info->name, info->name);

    ZVAL_STRINGL(&tmp, buf, buf_len);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_NAME, &tmp);
    zval_ptr_dtor(&tmp);

    ZVAL_STRING(&tmp, info->desc);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_DESC, &tmp);
    zval_ptr_dtor(&tmp);

    if (info->equiv != NULL) {
        ZVAL_STRING(&tmp, info->equiv);
    } else {
        ZVAL_NULL(&tmp);
    }
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_EQUIV, &tmp);
    zval_ptr_dtor(&tmp);

    ZVAL_LONG(&tmp, (zend_long) info->pmu);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_PMU, &tmp);

    ZVAL_LONG(&tmp, (zend_long) info->idx);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_IDX, &tmp);

    ZVAL_BOOL(&tmp, pmu_info->is_present);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_IS_PRESENT, &tmp);

    return SUCCESS;
}

ZEND_COLD
PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_get_pmu_event_info(pfm_pmu_info_t *restrict pmu_info, int idx, zval *restrict return_value)
{
    pfm_event_info_t info = {0};
    info.size = sizeof(info);

    pfm_err_t pfm_err = pfm_get_event_info(idx, PFM_OS_PERF_EVENT, &info);
    if (pfm_err != PFM_SUCCESS) {
        zend_throw_exception_ex(
            perfidious_pmu_event_not_found_exception_ce,
            pfm_err,
            "libpfm: cannot get event info for %d: %s",
            idx,
            pfm_strerror(pfm_err)
        );
        return FAILURE;
    }

    return perfidious_pmu_event_info_ctor(pmu_info, &info, return_value);
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_always_inline zend_class_entry *register_class_PmuEventInfo(void)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\PmuEventInfo", NULL);
    class_entry = zend_register_internal_class(&ce);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_NAME,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING)
        );
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_DESC,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING)
        );
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_EQUIV,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING | MAY_BE_NULL)
        );
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_PMU,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
        );
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_IDX,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
        );
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_declare_typed_property(
            class_entry,
            PERFIDIOUS_INTERNED_IS_PRESENT,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_BOOL)
        );
    } while (false);

    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    return class_entry;
}

PERFIDIOUS_LOCAL
void perfidious_pmu_event_info_minit(void)
{
    PERFIDIOUS_INTERNED_NAME = zend_string_init_interned(ZEND_STRL("name"), 1);
    PERFIDIOUS_INTERNED_DESC = zend_string_init_interned(ZEND_STRL("desc"), 1);
    PERFIDIOUS_INTERNED_EQUIV = zend_string_init_interned(ZEND_STRL("equiv"), 1);
    PERFIDIOUS_INTERNED_PMU = zend_string_init_interned(ZEND_STRL("pmu"), 1);
    PERFIDIOUS_INTERNED_IDX = zend_string_init_interned(ZEND_STRL("idx"), 1);
    PERFIDIOUS_INTERNED_IS_PRESENT = zend_string_init_interned(ZEND_STRL("is_present"), 1);

    perfidious_pmu_event_info_ce = register_class_PmuEventInfo();
}
