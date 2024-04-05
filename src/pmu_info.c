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
#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include "php_perf.h"

PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_NAME;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_DESC;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_PMU;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_TYPE;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_NEVENTS;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_IS_PRESENT;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_info_ce;

ZEND_COLD
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static inline zend_result perfidious_pmu_info_ctor(pfm_pmu_info_t *pmu_info, zval *restrict return_value)
{
    zval tmp = {0};

    if (UNEXPECTED(FAILURE == object_init_ex(return_value, perfidious_pmu_info_ce))) {
        return FAILURE;
    }

    ZVAL_STRING(&tmp, pmu_info->name);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_NAME, &tmp);
    zval_ptr_dtor(&tmp);

    ZVAL_STRING(&tmp, pmu_info->desc);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_DESC, &tmp);
    zval_ptr_dtor(&tmp);

    ZVAL_LONG(&tmp, (zend_long) pmu_info->pmu);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_PMU, &tmp);

    ZVAL_LONG(&tmp, (zend_long) pmu_info->type);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_TYPE, &tmp);

    ZVAL_LONG(&tmp, (zend_long) pmu_info->nevents);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_NEVENTS, &tmp);

    ZVAL_BOOL(&tmp, (zend_bool) pmu_info->is_present);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_IS_PRESENT, &tmp);

    return SUCCESS;
}

ZEND_COLD
PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_get_pmu_info(zend_long pmu, zval *restrict return_value, bool silent)
{
    pfm_pmu_info_t pmu_info = {0};
    pmu_info.size = sizeof(pmu_info);

    int pfm_err = pfm_get_pmu_info(pmu, &pmu_info);

    if (UNEXPECTED(pfm_err != PFM_SUCCESS)) {
        if (!silent) {
            zend_throw_exception_ex(
                perfidious_pmu_not_found_exception_ce,
                pfm_err,
                "cannot get pmu info for %lu: %s",
                pmu,
                pfm_strerror(pfm_err)
            );
        }
        return FAILURE;
    }

    return perfidious_pmu_info_ctor(&pmu_info, return_value);
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_always_inline zend_class_entry *register_class_PmuInfo(void)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERF_NAMESPACE "\\PmuInfo", NULL);
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
            PERFIDIOUS_INTERNED_TYPE,
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
            PERFIDIOUS_INTERNED_NEVENTS,
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
void perfidious_pmu_info_minit(void)
{
    PERFIDIOUS_INTERNED_NAME = zend_string_init_interned(ZEND_STRL("name"), 1);
    PERFIDIOUS_INTERNED_DESC = zend_string_init_interned(ZEND_STRL("desc"), 1);
    PERFIDIOUS_INTERNED_PMU = zend_string_init_interned(ZEND_STRL("pmu"), 1);
    PERFIDIOUS_INTERNED_TYPE = zend_string_init_interned(ZEND_STRL("type"), 1);
    PERFIDIOUS_INTERNED_NEVENTS = zend_string_init_interned(ZEND_STRL("nevents"), 1);
    PERFIDIOUS_INTERNED_IS_PRESENT = zend_string_init_interned(ZEND_STRL("is_present"), 1);

    perfidious_pmu_info_ce = register_class_PmuInfo();
}
