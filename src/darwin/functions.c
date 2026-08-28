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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <libproc.h>

#include "Zend/zend_API.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_long.h"
#include "main/php.h"

#include "php_perfidious.h"

#define PHP_PERFIDIOUS_DARWIN_NAMESPACE PHP_PERFIDIOUS_NAMESPACE "\\Darwin"

static zend_class_entry *perfidious_darwin_process_resource_usage_ce;

// clang-format off
ZEND_BEGIN_ARG_INFO_EX(perfidious_darwin_result_construct_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousDarwinResult, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

static const zend_function_entry perfidious_darwin_result_methods[] = {
    PHP_ME(PerfidiousDarwinResult, __construct, perfidious_darwin_result_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_FE_END
};
// clang-format on

static void
perfidious_darwin_declare_readonly_long_property(zend_class_entry *class_entry, const char *name, size_t name_length)
{
    zend_string *property_name = zend_string_init_interned(name, name_length, true);
    zval default_value;

    ZVAL_UNDEF(&default_value);
    zend_declare_typed_property(
        class_entry,
        property_name,
        &default_value,
        ZEND_ACC_PUBLIC | ZEND_ACC_READONLY,
        NULL,
        (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG)
    );
}

#define PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(class_entry, name)                                            \
    perfidious_darwin_declare_readonly_long_property(class_entry, ZEND_STRL(name))

static bool perfidious_darwin_uint64_to_zend_long(const char *field_name, uint64_t value, zend_long *result)
{
    if (UNEXPECTED(value > (uint64_t) ZEND_LONG_MAX)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "Darwin %s value 0x%016llx is too large to represent as a PHP integer",
            field_name,
            (unsigned long long) value
        );
        return false;
    }

    *result = (zend_long) value;
    return true;
}

static void perfidious_darwin_throw_errno(const char *function_name, int error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce, (zend_long) error, "%s failed: %s", function_name, strerror(error)
    );
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_darwin_get_current_process_resource_usage_arginfo,
    false,
    0,
    Perfidious\\Darwin\\ProcessResourceUsage,
    false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_darwin_get_current_process_resource_usage)
{
    struct rusage_info_v4 process_usage;
    struct rusage basic_usage;
    zend_long values[8];

    ZEND_PARSE_PARAMETERS_NONE();

    memset(&process_usage, 0, sizeof(process_usage));
    if (UNEXPECTED(proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *) &process_usage) != 0)) {
        int error = errno;
        perfidious_darwin_throw_errno("proc_pid_rusage", error);
        return;
    }

    memset(&basic_usage, 0, sizeof(basic_usage));
    if (UNEXPECTED(getrusage(RUSAGE_SELF, &basic_usage) != 0)) {
        int error = errno;
        perfidious_darwin_throw_errno("getrusage", error);
        return;
    }

    if (!perfidious_darwin_uint64_to_zend_long("user time", process_usage.ri_user_time, &values[0]) ||
        !perfidious_darwin_uint64_to_zend_long("system time", process_usage.ri_system_time, &values[1]) ||
        !perfidious_darwin_uint64_to_zend_long(
            "minor page fault count", (uint64_t) basic_usage.ru_minflt, &values[2]
        ) ||
        !perfidious_darwin_uint64_to_zend_long(
            "major page fault count", (uint64_t) basic_usage.ru_majflt, &values[3]
        ) ||
        !perfidious_darwin_uint64_to_zend_long(
            "voluntary context switch count", (uint64_t) basic_usage.ru_nvcsw, &values[4]
        ) ||
        !perfidious_darwin_uint64_to_zend_long(
            "involuntary context switch count", (uint64_t) basic_usage.ru_nivcsw, &values[5]
        ) ||
        !perfidious_darwin_uint64_to_zend_long("instruction count", process_usage.ri_instructions, &values[6]) ||
        !perfidious_darwin_uint64_to_zend_long("cycle count", process_usage.ri_cycles, &values[7])) {
        return;
    }

    object_init_ex(return_value, perfidious_darwin_process_resource_usage_ce);
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("userTimeNs"), values[0]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("systemTimeNs"), values[1]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("minorPageFaultCount"), values[2]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("majorPageFaultCount"), values[3]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("voluntaryContextSwitchCount"),
        values[4]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce,
        Z_OBJ_P(return_value),
        ZEND_STRL("involuntaryContextSwitchCount"),
        values[5]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("instructionCount"), values[6]
    );
    zend_update_property_long(
        perfidious_darwin_process_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("cycleCount"), values[7]
    );
}

#if PHP_VERSION_ID >= 80400
#define PERFIDIOUS_DARWIN_FE(zend_name, name, arg_info, flags)                                                         \
    ZEND_RAW_FENTRY(zend_name, name, arg_info, flags, NULL, NULL)
#else
#define PERFIDIOUS_DARWIN_FE(zend_name, name, arg_info, flags) ZEND_RAW_FENTRY(zend_name, name, arg_info, flags)
#endif

// clang-format off
PERFIDIOUS_LOCAL
const zend_function_entry perfidious_darwin_functions[] = {
    PERFIDIOUS_DARWIN_FE(PHP_PERFIDIOUS_DARWIN_NAMESPACE "\\get_current_process_resource_usage", ZEND_FN(perfidious_darwin_get_current_process_resource_usage), perfidious_darwin_get_current_process_resource_usage_arginfo, 0)
    PHP_FE_END
};
// clang-format on

PERFIDIOUS_LOCAL void perfidious_darwin_minit(void)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_DARWIN_NAMESPACE "\\ProcessResourceUsage", perfidious_darwin_result_methods);
    perfidious_darwin_process_resource_usage_ce = zend_register_internal_class(&ce);
    perfidious_darwin_process_resource_usage_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_process_resource_usage_ce, "userTimeNs");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_process_resource_usage_ce, "systemTimeNs");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(
        perfidious_darwin_process_resource_usage_ce, "minorPageFaultCount"
    );
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(
        perfidious_darwin_process_resource_usage_ce, "majorPageFaultCount"
    );
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(
        perfidious_darwin_process_resource_usage_ce, "voluntaryContextSwitchCount"
    );
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(
        perfidious_darwin_process_resource_usage_ce, "involuntaryContextSwitchCount"
    );
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_process_resource_usage_ce, "instructionCount");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_process_resource_usage_ce, "cycleCount");
}
