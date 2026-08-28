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

#include <dlfcn.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
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
#define PERFIDIOUS_DARWIN_THSC_TIME_CPI 3

static zend_class_entry *perfidious_darwin_process_resource_usage_ce;
static zend_class_entry *perfidious_darwin_thread_resource_usage_ce;

/*
 * thread_selfcounts is Apple SPI introduced in macOS 12.4. Resolve it at runtime so the extension
 * continues to load on older releases, where the public THREAD_BASIC_INFO API still provides CPU time.
 */
struct perfidious_darwin_thread_time_cpi
{
    uint64_t instructions;
    uint64_t cycles;
    uint64_t user_time_mach;
    uint64_t system_time_mach;
};

typedef int (*perfidious_darwin_thread_selfcounts_fn)(uint32_t kind, void *destination, size_t size);

static perfidious_darwin_thread_selfcounts_fn perfidious_darwin_thread_selfcounts;
static mach_timebase_info_data_t perfidious_darwin_timebase;

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

static void perfidious_darwin_throw_mach(const char *function_name, kern_return_t error)
{
    zend_throw_exception_ex(
        perfidious_io_exception_ce, (zend_long) error, "%s failed with Mach error %d", function_name, (int) error
    );
}

static bool perfidious_darwin_mach_time_to_ns(uint64_t value, uint64_t *result)
{
    uint64_t quotient;
    uint64_t remainder;
    uint64_t whole;
    uint64_t fraction;

    if (UNEXPECTED(perfidious_darwin_timebase.numer == 0 || perfidious_darwin_timebase.denom == 0)) {
        zend_throw_exception_ex(perfidious_io_exception_ce, 0, "mach_timebase_info returned an invalid ratio");
        return false;
    }

    quotient = value / perfidious_darwin_timebase.denom;
    remainder = value % perfidious_darwin_timebase.denom;

    if (UNEXPECTED(quotient > UINT64_MAX / perfidious_darwin_timebase.numer)) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "Darwin thread CPU time is too large to scale");
        return false;
    }

    whole = quotient * perfidious_darwin_timebase.numer;
    fraction = (remainder * (uint64_t) perfidious_darwin_timebase.numer) / perfidious_darwin_timebase.denom;

    if (UNEXPECTED(whole > UINT64_MAX - fraction)) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "Darwin thread CPU time is too large to scale");
        return false;
    }

    *result = whole + fraction;
    return true;
}

static bool perfidious_darwin_time_value_to_ns(const time_value_t *value, uint64_t *result)
{
    uint64_t seconds;
    uint64_t microseconds;

    if (UNEXPECTED(value->seconds < 0 || value->microseconds < 0 || value->microseconds >= 1000000)) {
        zend_throw_exception_ex(perfidious_io_exception_ce, 0, "thread_info returned an invalid CPU time");
        return false;
    }

    seconds = (uint64_t) value->seconds;
    microseconds = (uint64_t) value->microseconds;

    if (UNEXPECTED(seconds > (UINT64_MAX - microseconds * 1000) / 1000000000)) {
        zend_throw_exception_ex(perfidious_overflow_exception_ce, 0, "Darwin thread CPU time is too large to scale");
        return false;
    }

    *result = seconds * 1000000000 + microseconds * 1000;
    return true;
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

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(
    perfidious_darwin_get_current_thread_resource_usage_arginfo,
    false,
    0,
    Perfidious\\Darwin\\ThreadResourceUsage,
    false
)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(perfidious_darwin_get_current_thread_resource_usage)
{
    struct perfidious_darwin_thread_time_cpi usage;
    uint64_t native_values[4] = {0};
    zend_long values[4];
    bool have_usage = false;

    ZEND_PARSE_PARAMETERS_NONE();

    if (perfidious_darwin_thread_selfcounts != NULL) {
        memset(&usage, 0, sizeof(usage));
        if (perfidious_darwin_thread_selfcounts(PERFIDIOUS_DARWIN_THSC_TIME_CPI, &usage, sizeof(usage)) == 0) {
            if (!perfidious_darwin_mach_time_to_ns(usage.user_time_mach, &native_values[0]) ||
                !perfidious_darwin_mach_time_to_ns(usage.system_time_mach, &native_values[1])) {
                return;
            }

            native_values[2] = usage.instructions;
            native_values[3] = usage.cycles;
            have_usage = true;
        } else if (errno != ENOTSUP && errno != ENOSYS) {
            int error = errno;
            perfidious_darwin_throw_errno("thread_selfcounts", error);
            return;
        }
    }

    if (!have_usage) {
        thread_basic_info_data_t basic_usage;
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
        thread_t thread = mach_thread_self();
        kern_return_t error;

        memset(&basic_usage, 0, sizeof(basic_usage));
        error = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t) &basic_usage, &count);
        (void) mach_port_deallocate(mach_task_self(), thread);

        if (UNEXPECTED(error != KERN_SUCCESS)) {
            perfidious_darwin_throw_mach("thread_info", error);
            return;
        }

        if (!perfidious_darwin_time_value_to_ns(&basic_usage.user_time, &native_values[0]) ||
            !perfidious_darwin_time_value_to_ns(&basic_usage.system_time, &native_values[1])) {
            return;
        }
    }

    if (!perfidious_darwin_uint64_to_zend_long("thread user time", native_values[0], &values[0]) ||
        !perfidious_darwin_uint64_to_zend_long("thread system time", native_values[1], &values[1]) ||
        !perfidious_darwin_uint64_to_zend_long("thread instruction count", native_values[2], &values[2]) ||
        !perfidious_darwin_uint64_to_zend_long("thread cycle count", native_values[3], &values[3])) {
        return;
    }

    object_init_ex(return_value, perfidious_darwin_thread_resource_usage_ce);
    zend_update_property_long(
        perfidious_darwin_thread_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("userTimeNs"), values[0]
    );
    zend_update_property_long(
        perfidious_darwin_thread_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("systemTimeNs"), values[1]
    );
    zend_update_property_long(
        perfidious_darwin_thread_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("instructionCount"), values[2]
    );
    zend_update_property_long(
        perfidious_darwin_thread_resource_usage_ce, Z_OBJ_P(return_value), ZEND_STRL("cycleCount"), values[3]
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
    PERFIDIOUS_DARWIN_FE(PHP_PERFIDIOUS_DARWIN_NAMESPACE "\\get_current_thread_resource_usage", ZEND_FN(perfidious_darwin_get_current_thread_resource_usage), perfidious_darwin_get_current_thread_resource_usage_arginfo, 0)
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

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_DARWIN_NAMESPACE "\\ThreadResourceUsage", perfidious_darwin_result_methods);
    perfidious_darwin_thread_resource_usage_ce = zend_register_internal_class(&ce);
    perfidious_darwin_thread_resource_usage_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_thread_resource_usage_ce, "userTimeNs");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_thread_resource_usage_ce, "systemTimeNs");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_thread_resource_usage_ce, "instructionCount");
    PERFIDIOUS_DARWIN_DECLARE_READONLY_LONG_PROPERTY(perfidious_darwin_thread_resource_usage_ce, "cycleCount");

    {
        union
        {
            void *object;
            perfidious_darwin_thread_selfcounts_fn function;
        } symbol;

        symbol.object = dlsym(RTLD_DEFAULT, "thread_selfcounts");
        perfidious_darwin_thread_selfcounts = symbol.function;
    }

    if (perfidious_darwin_thread_selfcounts != NULL &&
        UNEXPECTED(mach_timebase_info(&perfidious_darwin_timebase) != KERN_SUCCESS)) {
        perfidious_darwin_thread_selfcounts = NULL;
    }
}
