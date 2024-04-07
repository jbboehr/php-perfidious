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

#ifndef PHP_PERFIDIOUS_H
#define PHP_PERFIDIOUS_H

#include <sys/types.h>
#include "main/php.h"

#define PHP_PERFIDIOUS_NAME "perfidious"
#define PHP_PERFIDIOUS_VERSION "0.1.0"
#define PHP_PERFIDIOUS_RELEASE "2024-03-24"
#define PHP_PERFIDIOUS_AUTHORS "John Boehr <jbboehr@gmail.com> (lead)"
#define PHP_PERFIDIOUS_NAMESPACE "Perfidious"

#if (__GNUC__ >= 4) || defined(__clang__) || defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define PERFIDIOUS_PUBLIC __attribute__((visibility("default")))
#define PERFIDIOUS_LOCAL __attribute__((visibility("hidden")))
#elif defined(PHP_WIN32) && defined(PERF_EXPORTS)
#define PERFIDIOUS_PUBLIC __declspec(dllexport)
#define PERFIDIOUS_LOCAL
#else
#define PERFIDIOUS_PUBLIC
#define PERFIDIOUS_LOCAL
#endif

#if (__GNUC__ >= 3) || defined(__clang__)
#define PERFIDIOUS_ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define PERFIDIOUS_ATTR_NONNULL_ALL __attribute__((nonnull))
#define PERFIDIOUS_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define PERFIDIOUS_ATTR_NONNULL(...)
#define PERFIDIOUS_ATTR_NONNULL_ALL
#define PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
#endif

#if ((__GNUC__ >= 5) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ >= 9))) || defined(__clang__)
#define PERFIDIOUS_ATTR_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define PERFIDIOUS_ATTR_RETURNS_NONNULL
#endif

#ifndef ZEND_HOT
#if defined(__GNUC__) && ZEND_GCC_VERSION >= 4003
#define ZEND_HOT __attribute__((hot))
#else
#define ZEND_HOT
#endif
#endif

extern zend_module_entry perfidious_module_entry;
#define phpext_perfidious_ptr &perfidious_module_entry

#if defined(ZTS) && ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && defined(COMPILE_DL_PERFIDIOUS)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

struct perfidious_handle;

enum perfidious_error_mode
{
    PERFIDIOUS_ERROR_MODE_THROW = 0,
    PERFIDIOUS_ERROR_MODE_WARNING = 1,
};

enum perfidious_overflow_mode
{
    PERFIDIOUS_OVERFLOW_THROW = 0,
    PERFIDIOUS_OVERFLOW_WARN = 1,
    PERFIDIOUS_OVERFLOW_SATURATE = 2,
    PERFIDIOUS_OVERFLOW_WRAP = 3,
    PERFIDIOUS_OVERFLOW_MAX = 3,
};

PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_exception_interface_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_pmu_not_found_exception_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_pmu_event_not_found_exception_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_overflow_exception_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_io_exception_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_pmu_event_info_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_pmu_info_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_handle_ce;
PERFIDIOUS_PUBLIC extern zend_class_entry *perfidious_read_result_ce;

ZEND_BEGIN_MODULE_GLOBALS(perfidious)
    zend_bool global_enable;
    zend_string *global_metrics;
    struct perfidious_handle *global_handle;

    zend_bool request_enable;
    zend_string *request_metrics;
    struct perfidious_handle *request_handle;

    enum perfidious_error_mode error_mode;
    enum perfidious_overflow_mode overflow_mode;
ZEND_END_MODULE_GLOBALS(perfidious)

ZEND_EXTERN_MODULE_GLOBALS(perfidious);

#define PERFIDIOUS_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(perfidious, v)

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_reset(struct perfidious_handle *restrict handle);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_enable(struct perfidious_handle *restrict handle);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_disable(struct perfidious_handle *restrict handle);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_close(struct perfidious_handle *restrict handle);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *
perfidious_handle_open(zend_string **restrict event_names, size_t event_names_length, bool persist);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *perfidious_handle_open_ex(
    zend_string **restrict event_names, size_t event_names_length, pid_t pid, int cpu, bool persist
);

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
size_t perfidious_handle_read_buffer_size(const struct perfidious_handle *restrict handle);

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result
perfidious_handle_read_raw(const struct perfidious_handle *restrict handle, size_t size, void *restrict buffer);

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result
perfidious_handle_read_to_array(const struct perfidious_handle *restrict handle, zval *restrict return_value);

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_to_array_with_times(
    const struct perfidious_handle *restrict handle,
    zval *restrict return_value,
    uint64_t *restrict time_enabled,
    uint64_t *restrict time_running
);

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result
perfidious_handle_read_to_result(const struct perfidious_handle *restrict handle, zval *restrict return_value);

#endif /* PHP_PERFIDIOUS_H */
