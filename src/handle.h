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

#ifndef PERFIDIOUS_HANDLE_H
#define PERFIDIOUS_HANDLE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "php_perf.h"

static const uint64_t PERFIDIOUS_HANDLE_MARKER = 0x327b23c66b8b4567;

struct perfidious_metric
{
    int fd;
    uint64_t id;
    zend_string *name;
};

struct perfidious_handle
{
    uint64_t marker;
    size_t metrics_size;
    size_t metrics_count;
    bool enabled;
    struct perfidious_metric metrics[];
};

struct perfidious_read_format_value
{
    uint64_t value;
    uint64_t id;
};

struct perfidious_read_format
{
    uint64_t nr;
    uint64_t time_enabled;
    uint64_t time_running;
    struct perfidious_read_format_value values[];
};

struct perfidious_handle_obj
{
    struct perfidious_handle *handle;
    bool no_auto_close;
    zend_object std;
};

ZEND_HOT
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_RETURNS_NONNULL
static inline struct perfidious_handle_obj *perfidious_fetch_handle_object(zend_object *obj)
{
    return (struct perfidious_handle_obj *) ((char *) (obj) -XtOffsetOf(struct perfidious_handle_obj, std));
}
#define Z_PERF_HANDLE_P(zv) perfidious_fetch_handle_object(Z_OBJ_P((zv)))

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_disable_arginfo, 0, 0, PerfExt\\Handle, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_enable_arginfo, 0, 0, PerfExt\\Handle, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_read_arginfo, 0, 0, PerfExt\\ReadResult, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_handle_read_array_arginfo, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_reset_arginfo, 0, 0, PerfExt\\Handle, 0)
ZEND_END_ARG_INFO()

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_to_array(struct perfidious_handle *handle, zval *return_value);

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_to_array_with_times(
    struct perfidious_handle *restrict handle,
    zval *restrict return_value,
    uint64_t *restrict time_enabled,
    uint64_t *restrict time_running
);

ZEND_HOT
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_always_inline zend_result perfidious_handle_marker_assert(struct perfidious_handle *restrict handle)
{
    if (UNEXPECTED(handle->marker != PERFIDIOUS_HANDLE_MARKER)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "Assertion failed: marker mismatch: %" PRIu64 " != %" PRIu64,
            handle->marker,
            PERFIDIOUS_HANDLE_MARKER
        );
        return FAILURE;
    }

    return SUCCESS;
}

#endif /* PERFIDIOUS_HANDLE_H */
