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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <inttypes.h>
#include <err.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_portability.h>
#include "main/php.h"
#include "php_perf.h"
#include "./handle.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_handle_ce;
static zend_object_handlers perfidious_handle_obj_handlers;

static void perfidious_handle_ioctl_error(void)
{
    zend_throw_exception_ex(perfidious_io_exception_ce, errno, "ioctl failed: %s", strerror(errno));
}

PERFIDIOUS_ATTR_NONNULL_ALL
static void perfidious_handle_obj_free(zend_object *object)
{
    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(object);

    if (obj->handle && !obj->no_auto_close) {
        perfidious_handle_close(obj->handle);
        obj->handle = NULL;
    }

    zend_object_std_dtor((zend_object *) object);
}

PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_RETURNS_NONNULL
static zend_object *perfidious_handle_obj_create(zend_class_entry *ce)
{
    struct perfidious_handle_obj *obj;

    obj = ecalloc(1, sizeof(*obj) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &perfidious_handle_obj_handlers;

    return &obj->std;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
void perfidious_handle_reset(struct perfidious_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        perfidious_handle_ioctl_error();
    }
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
void perfidious_handle_enable(struct perfidious_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        perfidious_handle_ioctl_error();
    }

    handle->enabled = true;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
void perfidious_handle_disable(struct perfidious_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        perfidious_handle_ioctl_error();
    }

    handle->enabled = false;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
void perfidious_handle_close(struct perfidious_handle *handle)
{
    for (ssize_t i = (ssize_t) handle->metrics_count - 1; i >= 0; i--) {
        int err = close(handle->metrics[i].fd);
        if (err == -1) {
            zend_throw_exception_ex(perfidious_io_exception_ce, errno, "close failed: %s", strerror(errno));
            // continue even if it fails
        }
        if (EXPECTED(handle->metrics[i].name)) {
            zend_string_release(handle->metrics[i].name);
        }
    }

    handle->metrics_count = 0;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_read_to_array(struct perfidious_handle *handle, zval *return_value)
{
    zend_result rv = SUCCESS;
    bool orig_enabled = handle->enabled;

    if (orig_enabled) {
        perfidious_handle_disable(handle);
    }

    size_t size = sizeof(struct perfidious_read_format) +
                  sizeof(((struct perfidious_read_format){0}).values[0]) * handle->metrics_count;
    struct perfidious_read_format *data = alloca(size);

    ssize_t bytes_read = read(handle->metrics[0].fd, (void *) data, size);

    if (bytes_read == -1) {
        rv = FAILURE;
        ZVAL_UNDEF(return_value);
        zend_throw_exception_ex(perfidious_io_exception_ce, errno, "failed to read: %s", strerror(errno));
        goto done;
    }

    array_init(return_value);

    for (size_t i = 0; i < data->nr; i++) {
        struct perfidious_metric *e = &handle->metrics[i];

        if (UNEXPECTED(e->id != data->values[i].id)) {
            e = NULL;

            // skip the first entry - it should be the dummy
            for (size_t j = 1; j < handle->metrics_count; j++) {
                if (handle->metrics[j].id == data->values[i].id) {
                    e = &handle->metrics[j];
                    break;
                }
            }

            if (UNEXPECTED(e == NULL)) {
                continue;
            }
        } else if (i == 0) {
            // skip the first entry - it should be the dummy
            continue;
        }

        if (EXPECTED(e->name != NULL)) {
            if (UNEXPECTED(data->values[i].value > (uint64_t) ZEND_LONG_MAX)) {
                zend_throw_exception_ex(
                    perfidious_overflow_exception_ce, 0, "counter truncation for %.*s", (int) e->name->len, e->name->val
                );
                ZVAL_UNDEF(return_value);
                rv = FAILURE;
                goto done;
            }

            add_assoc_long_ex(return_value, e->name->val, e->name->len, (zend_long) data->values[i].value);
        }
    }

done:
    if (orig_enabled) {
        perfidious_handle_enable(handle);
    }

    return rv;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *perfidious_handle_open(zend_string **event_names, size_t event_names_length, bool persist)
{
    return perfidious_handle_open_ex(event_names, event_names_length, 0, -1, persist);
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *
perfidious_handle_open_ex(zend_string **event_names, size_t event_names_length, pid_t pid, int cpu, bool persist)
{
    int fd;
    uint64_t id;
    int group_fd;
    int err;

    struct perfidious_handle *handle = pecalloc(
        sizeof(struct perfidious_handle) + sizeof(struct perfidious_metric) * (event_names_length + 1), 1, persist
    );
    handle->marker = PHP_PERF_HANDLE_MARKER;
    handle->metrics_size = event_names_length + 1;

    // Open a dummy event to hold the group
    do {
        struct perf_event_attr attr = {
            .type = PERF_TYPE_SOFTWARE,
            .config = PERF_COUNT_SW_DUMMY,
            .size = sizeof(attr),
            .disabled = 1,
            .watermark = 0,
            .exclude_kernel = 1,
            .exclude_hv = 1,
            .read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID,
        };
        fd = (int) perf_event_open(&attr, pid, cpu, -1, 0);
        if (UNEXPECTED(fd == -1)) {
            zend_throw_exception_ex(
                perfidious_io_exception_ce,
                errno,
                "perf_event_open() failed for perf::PERF_COUNT_SW_DUMMY: %s",
                strerror(errno)
            );
            goto cleanup;
        }
        err = ioctl(fd, PERF_EVENT_IOC_ID, &id);
        if (err == -1) {
            perfidious_handle_ioctl_error();
            close(fd);
            goto cleanup;
        }
        err = ioctl(fd, PERF_EVENT_IOC_RESET, fd);
        if (err == -1) {
            perfidious_handle_ioctl_error();
            close(fd);
            goto cleanup;
        }
        handle->metrics[handle->metrics_count++] = (struct perfidious_metric){
            .fd = fd,
            .id = id,
            .name = zend_string_init(ZEND_STRL("PERF_COUNT_SW_DUMMY"), persist),
        };
        group_fd = fd;
    } while (false);

    // Open the other events
    for (size_t i = 0; i < event_names_length; i++) {
        zend_string *event_name = event_names[i];
        struct perf_event_attr attr = {0};
        pfm_perf_encode_arg_t arg = {0};
        arg.attr = &attr;
        arg.size = sizeof(arg);

        err = pfm_get_os_event_encoding(ZSTR_VAL(event_name), PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
        if (UNEXPECTED(err != PFM_SUCCESS)) {
            zend_throw_exception_ex(
                perfidious_pmu_event_not_found_exception_ce,
                err,
                "failed to get libpfm event encoding for %.*s: %s",
                (int) ZSTR_LEN(event_name),
                ZSTR_VAL(event_name),
                pfm_strerror(err)
            );
            goto cleanup;
        }

        attr.size = sizeof(attr);
        attr.disabled = 0;
        attr.watermark = 0;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

        fd = (int) perf_event_open(&attr, pid, cpu, group_fd, 0);

        if (UNEXPECTED(fd == -1)) {
            zend_throw_exception_ex(
                perfidious_io_exception_ce,
                errno,
                "perf_event_open() failed for %.*s: %s",
                (int) ZSTR_LEN(event_name),
                ZSTR_VAL(event_name),
                strerror(errno)
            );
            goto cleanup;
        }

        err = ioctl(fd, PERF_EVENT_IOC_ID, &id);
        if (err == -1) {
            perfidious_handle_ioctl_error();
            close(fd);
            goto cleanup;
        }

        err = ioctl(fd, PERF_EVENT_IOC_RESET, fd);
        if (err == -1) {
            perfidious_handle_ioctl_error();
            close(fd);
            goto cleanup;
        }

        ZEND_ASSERT(handle->metrics_count < handle->metrics_size);

        handle->metrics[handle->metrics_count++] = (struct perfidious_metric){
            .fd = fd,
            .id = id,
            // can't use zend_string_copy because it doesn't persist?
            .name = zend_string_dup(event_name, persist),
        };
    }

    return handle;

cleanup:
    perfidious_handle_close(handle);
    return NULL;
}

static PHP_METHOD(Handle, disable)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    perfidious_handle_disable(obj->handle);

    RETURN_ZVAL(self, 1, 0);
}

static PHP_METHOD(Handle, enable)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    perfidious_handle_enable(obj->handle);

    RETURN_ZVAL(self, 1, 0);
}

ZEND_HOT
static PHP_METHOD(Handle, read)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    perfidious_handle_read_to_array(obj->handle, return_value);
}

static PHP_METHOD(Handle, reset)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    perfidious_handle_reset(obj->handle);

    RETURN_ZVAL(self, 1, 0);
}

// clang-format off
static zend_function_entry perfidious_handle_methods[] = {
    PHP_ME(Handle, disable, perfidious_handle_disable_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Handle, enable, perfidious_handle_enable_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Handle, read, perfidious_handle_read_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(Handle, reset, perfidious_handle_reset_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
// clang-format on

PERFIDIOUS_ATTR_RETURNS_NONNULL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
static zend_always_inline zend_class_entry *register_class_Handle(void)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    memcpy(&perfidious_handle_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    perfidious_handle_obj_handlers.offset = XtOffsetOf(struct perfidious_handle_obj, std);
    perfidious_handle_obj_handlers.free_obj = perfidious_handle_obj_free;
    perfidious_handle_obj_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(ce, PHP_PERF_NAMESPACE "\\Handle", perfidious_handle_methods);
    class_entry = zend_register_internal_class(&ce);

    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    class_entry->create_object = perfidious_handle_obj_create;

    return class_entry;
}

PERFIDIOUS_LOCAL
zend_result perfidious_handle_minit(void)
{
    perfidious_handle_ce = register_class_Handle();

    return SUCCESS;
}
