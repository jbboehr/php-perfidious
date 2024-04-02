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
#include "php_perf.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_info_ce;

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
        zend_string *name = zend_string_init_interned("name", sizeof("name") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING)
        );
        zend_string_release(name);
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_string *name = zend_string_init_interned("desc", sizeof("desc") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING)
        );
        zend_string_release(name);
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_string *name = zend_string_init_interned("pmu", sizeof("pmu") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
        );
        zend_string_release(name);
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_string *name = zend_string_init_interned("type", sizeof("type") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
        );
        zend_string_release(name);
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_string *name = zend_string_init_interned("nevents", sizeof("nevents") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
        );
        zend_string_release(name);
    } while (false);

    do {
        zval default_value = {0};
        ZVAL_UNDEF(&default_value);
        zend_string *name = zend_string_init_interned("is_present", sizeof("is_present") - 1, 1);
        zend_declare_typed_property(
            class_entry,
            name,
            &default_value,
            ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
            NULL,
            (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_BOOL)
        );
        zend_string_release(name);
    } while (false);

    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    return class_entry;
}

PERFIDIOUS_LOCAL zend_result perfidious_pmu_info_minit(void)
{
    perfidious_pmu_info_ce = register_class_PmuInfo();

    return SUCCESS;
}
