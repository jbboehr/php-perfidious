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

PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_NAME;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_DESC;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_PMU;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_TYPE;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_NEVENTS;
PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_IS_PRESENT;
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
