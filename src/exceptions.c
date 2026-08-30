/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3,
 * as published by the Free Software Foundation, together with the Romic
 * Exception (an additional permission under section 7 of that license).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * and the Romic Exception along with this program.  If not, see
 * <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Zend/zend_API.h>
#include <ext/spl/spl_exceptions.h>
#include "php_perfidious.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_exception_interface_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_event_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_overflow_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_io_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_unsupported_metric_exception_ce;

// clang-format off
ZEND_BEGIN_ARG_INFO_EX(perfidious_unsupported_metric_exception_construct_arginfo, false, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousUnsupportedMetricException, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

static const zend_function_entry perfidious_unsupported_metric_exception_methods[] = {
    PHP_ME(
        PerfidiousUnsupportedMetricException,
        __construct,
        perfidious_unsupported_metric_exception_construct_arginfo,
        ZEND_ACC_PRIVATE
    )
    PHP_FE_END
};
// clang-format on

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_ExceptionInterface(void)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\ExceptionInterface", NULL);
    class_entry = zend_register_internal_interface(&ce);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_OverflowException(zend_class_entry *restrict iface)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\OverflowException", NULL);
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_OverflowException);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    zend_class_implements(class_entry, 1, iface);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_PmuNotFoundException(zend_class_entry *restrict iface)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\PmuNotFoundException", NULL);
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_InvalidArgumentException);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    zend_class_implements(class_entry, 1, iface);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_PmuEventNotFoundException(zend_class_entry *restrict iface)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\PmuEventNotFoundException", NULL);
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_InvalidArgumentException);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    zend_class_implements(class_entry, 1, iface);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_IOException(zend_class_entry *restrict iface)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\IOException", NULL);
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_RuntimeException);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;
    zend_class_implements(class_entry, 1, iface);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_UnsupportedMetricException(zend_class_entry *restrict iface)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;
    zend_string *property_name;
    zval default_value;

    INIT_CLASS_ENTRY(
        ce, PHP_PERFIDIOUS_NAMESPACE "\\UnsupportedMetricException", perfidious_unsupported_metric_exception_methods
    );
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_RuntimeException);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    zend_class_implements(class_entry, 1, iface);

    ZVAL_UNDEF(&default_value);
    property_name = zend_string_init_interned(ZEND_STRL("scope"), true);
    zend_declare_typed_property(
        class_entry,
        property_name,
        &default_value,
        ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
        NULL,
        (zend_type) ZEND_TYPE_INIT_CLASS(
            zend_string_init_interned(ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\Scope"), true), false, 0
        )
    );

    ZVAL_UNDEF(&default_value);
    property_name = zend_string_init_interned(ZEND_STRL("unsupportedMetrics"), true);
    zend_declare_typed_property(
        class_entry,
        property_name,
        &default_value,
        ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
        NULL,
        (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_ARRAY)
    );

    return class_entry;
}

PERFIDIOUS_LOCAL
void perfidious_exceptions_minit(void)
{
    perfidious_exception_interface_ce = register_class_ExceptionInterface();
    perfidious_overflow_exception_ce = register_class_OverflowException(perfidious_exception_interface_ce);
    perfidious_pmu_not_found_exception_ce = register_class_PmuNotFoundException(perfidious_exception_interface_ce);
    perfidious_pmu_event_not_found_exception_ce =
        register_class_PmuEventNotFoundException(perfidious_exception_interface_ce);
    perfidious_io_exception_ce = register_class_IOException(perfidious_exception_interface_ce);
    perfidious_unsupported_metric_exception_ce =
        register_class_UnsupportedMetricException(perfidious_exception_interface_ce);
}
