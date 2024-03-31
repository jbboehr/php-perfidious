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

#ifndef PHP_PERF_H
#define PHP_PERF_H

#include "main/php.h"

#define PHP_PERF_NAME "perf"
#define PHP_PERF_VERSION "0.1.0"
#define PHP_PERF_RELEASE "2024-03-24"
#define PHP_PERF_AUTHORS "John Boehr <jbboehr@gmail.com> (lead)"

#if (__GNUC__ >= 4) || defined(__clang__) || defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define PERF_PUBLIC __attribute__((visibility("default")))
#define PERF_LOCAL __attribute__((visibility("hidden")))
#elif defined(PHP_WIN32) && defined(PERF_EXPORTS)
#define PERF_PUBLIC __declspec(dllexport)
#define PERF_LOCAL
#else
#define PERF_PUBLIC
#define PERF_LOCAL
#endif

#if (__GNUC__ >= 3) || defined(__clang__)
#define PHP_PERF_ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define PHP_PERF_ATTR_NONNULL_ALL __attribute__((nonnull))
#define PHP_PERF_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define PHP_PERF_ATTR_NONNULL(...)
#define PHP_PERF_ATTR_NONNULL_ALL
#define PHP_PERF_ATTR_WARN_UNUSED_RESULT
#endif

#if ((__GNUC__ >= 5) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ >= 9))) || defined(__clang__)
#define PHP_PERF_ATTR_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define PHP_PERF_ATTR_RETURNS_NONNULL
#endif

extern zend_module_entry perf_module_entry;
#define phpext_perf_ptr &perf_module_entry

#if defined(ZTS) && ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && defined(COMPILE_DL_PERF)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

struct php_perf_handle;

PERF_PUBLIC extern zend_class_entry *perf_pmu_not_found_exception_ce;
PERF_PUBLIC extern zend_class_entry *perf_pmu_event_info_ce;
PERF_PUBLIC extern zend_class_entry *perf_pmu_info_ce;
PERF_PUBLIC extern zend_class_entry *perf_handle_ce;

ZEND_BEGIN_MODULE_GLOBALS(perf)
    zend_bool global_enable;
    zend_string *global_metrics;
    struct php_perf_handle *global_handle;

    zend_bool request_enable;
    zend_string *request_metrics;
    struct php_perf_handle *request_handle;
ZEND_END_MODULE_GLOBALS(perf)

ZEND_EXTERN_MODULE_GLOBALS(perf);

#define PERF_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(perf, v)

// php_perf_handle
struct php_perf_handle;
PERF_PUBLIC void php_perf_handle_reset(struct php_perf_handle *handle);
PERF_PUBLIC void php_perf_handle_enable(struct php_perf_handle *handle);
PERF_PUBLIC void php_perf_handle_disable(struct php_perf_handle *handle);
PERF_PUBLIC void php_perf_handle_close(struct php_perf_handle *handle);
PERF_PUBLIC PHP_PERF_ATTR_WARN_UNUSED_RESULT struct php_perf_handle *
php_perf_handle_open(zend_string **event_names, size_t event_names_length, bool persist);
PERF_PUBLIC
void php_perf_handle_read_to_array(struct php_perf_handle *handle, zval *return_value);

#endif /* PHP_PERF_H */
