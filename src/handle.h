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

#ifndef PHP_PERF_HANDLE_H
#define PHP_PERF_HANDLE_H

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
    struct perfidious_metric metrics[];
};

struct perfidious_read_format
{
    uint64_t nr;
    struct
    {
        uint64_t value;
        uint64_t id;
    } values[];
};

struct perfidious_handle_obj
{
    struct perfidious_handle *handle;
    bool no_auto_close;
    zend_object std;
};

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_handle_read_arginfo, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_handle_reset_arginfo, 0, 0, PerfExt\\Handle, 0)
ZEND_END_ARG_INFO()

static const uint64_t PHP_PERF_HANDLE_MARKER = 0x327b23c66b8b4567;

#endif /* PHP_PERF_HANDLE_H */
