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

#ifndef PERFIDIOUS_HANDLE_H
#define PERFIDIOUS_HANDLE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "php_perfidious.h"

struct perfidious_metric
{
    int fd;
    uint64_t id;
    zend_string *name;
};

struct perfidious_handle
{
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

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_disable_arginfo, 0, 0, Perfidious\\Handle, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_enable_arginfo, 0, 0, Perfidious\\Handle, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_handle_raw_stream_arginfo, IS_RESOURCE, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(false, idx, IS_LONG, true, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_read_arginfo, 0, 0, Perfidious\\ReadResult, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_handle_read_array_arginfo, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_reset_arginfo, 0, 0, Perfidious\\Handle, 0)
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

#endif /* PERFIDIOUS_HANDLE_H */
