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

#ifndef PERFIDIOUS_HANDLE_H
#define PERFIDIOUS_HANDLE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "php_perfidious.h"

// Bound temporary event-name arrays and descriptor groups in the PHP and INI entry points.
#define PERFIDIOUS_MAX_EVENT_NAMES 1000

struct perfidious_metric
{
    int fd;
    uint64_t id;
    zend_string *name;
};

struct perfidious_handle
{
    size_t metrics_count;
    bool enabled;
    bool persist;
    uint64_t time_enabled_at_reset;
    uint64_t time_running_at_reset;
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
    bool borrowed;
    zend_object std;
};

PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
int perfidious_handle_try_reset(struct perfidious_handle *restrict handle);

PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
int perfidious_handle_try_set_enabled(struct perfidious_handle *restrict handle, bool enabled);

PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
int perfidious_handle_try_close(struct perfidious_handle *restrict handle);

PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *perfidious_handle_try_open_ex(
    zend_string **restrict event_names,
    size_t event_names_length,
    pid_t pid,
    int cpu,
    bool persist,
    struct perfidious_error *error
);

ZEND_HOT
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_RETURNS_NONNULL
static inline struct perfidious_handle_obj *perfidious_fetch_handle_object(zend_object *obj)
{
    return (struct perfidious_handle_obj *) ((char *) (obj) -XtOffsetOf(struct perfidious_handle_obj, std));
}
#define Z_PERF_HANDLE_P(zv) perfidious_fetch_handle_object(Z_OBJ_P((zv)))

#endif /* PERFIDIOUS_HANDLE_H */
