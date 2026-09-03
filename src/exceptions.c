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
#include "zend_helpers.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_exception_interface_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_event_not_found_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_overflow_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_io_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_closed_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_wrong_thread_exception_ce;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_resource_busy_exception_ce;
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

static zend_class_entry *perfidious_register_exception(
    const char *name,
    size_t name_length,
    zend_class_entry *parent,
    zend_class_entry *iface,
    const zend_function_entry *methods,
    uint32_t extra_flags
)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY_EX(ce, name, name_length, methods);
    class_entry = zend_register_internal_class_ex(&ce, parent);
    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | extra_flags;
    zend_class_implements(class_entry, 1, iface);

    return class_entry;
}

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_class_entry *register_class_UnsupportedMetricException(zend_class_entry *restrict iface)
{
    zend_class_entry *class_entry;

    class_entry = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\UnsupportedMetricException"),
        spl_ce_RuntimeException,
        iface,
        perfidious_unsupported_metric_exception_methods,
        ZEND_ACC_NOT_SERIALIZABLE
    );
    perfidious_declare_readonly_property(
        class_entry,
        ZEND_STRL("scope"),
        (zend_type) ZEND_TYPE_INIT_CLASS(
            zend_string_init_interned(ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\Scope"), true), false, 0
        )
    );
    PERFIDIOUS_DECLARE_READONLY_PROPERTY(class_entry, "unsupportedMetrics", MAY_BE_ARRAY);

    return class_entry;
}

PERFIDIOUS_LOCAL
void perfidious_exceptions_minit(void)
{
    perfidious_exception_interface_ce = register_class_ExceptionInterface();
    perfidious_overflow_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\OverflowException"),
        spl_ce_OverflowException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_pmu_not_found_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\PmuNotFoundException"),
        spl_ce_InvalidArgumentException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_pmu_event_not_found_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\PmuEventNotFoundException"),
        spl_ce_InvalidArgumentException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_io_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\IOException"),
        spl_ce_RuntimeException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_closed_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\ClosedException"),
        spl_ce_LogicException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_wrong_thread_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\WrongThreadException"),
        spl_ce_LogicException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_resource_busy_exception_ce = perfidious_register_exception(
        ZEND_STRL(PHP_PERFIDIOUS_NAMESPACE "\\ResourceBusyException"),
        spl_ce_RuntimeException,
        perfidious_exception_interface_ce,
        NULL,
        0
    );
    perfidious_unsupported_metric_exception_ce =
        register_class_UnsupportedMetricException(perfidious_exception_interface_ce);
}
