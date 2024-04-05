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

#include <Zend/zend_API.h>
#include <ext/spl/spl_exceptions.h>
#include "php_perfidious.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_exception_interface_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_event_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_overflow_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_io_exception_ce;

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

PERFIDIOUS_LOCAL
void perfidious_exceptions_minit(void)
{
    perfidious_exception_interface_ce = register_class_ExceptionInterface();
    perfidious_overflow_exception_ce = register_class_OverflowException(perfidious_exception_interface_ce);
    perfidious_pmu_not_found_exception_ce = register_class_PmuNotFoundException(perfidious_exception_interface_ce);
    perfidious_pmu_event_not_found_exception_ce =
        register_class_PmuEventNotFoundException(perfidious_exception_interface_ce);
    perfidious_io_exception_ce = register_class_IOException(perfidious_exception_interface_ce);
}
