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

#include "Zend/zend_API.h"
#include "main/php.h"
#include "php_perf.h"
#include "./handle.h"

PERF_PUBLIC zend_class_entry *perf_handle_ce;
static zend_object_handlers php_perf_handle_obj_handlers;

static void handle_ioctl_error(void)
{
    php_error_docref(NULL, E_WARNING, "perf: ioctl: %s", strerror(errno));
}

static void php_perf_handle_obj_free(zend_object *object)
{
    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(object);

    if (obj->handle) {
        php_perf_handle_close(obj->handle);
        obj->handle = NULL;
    }

    zend_object_std_dtor((zend_object *) object);
}

static zend_object *php_perf_handle_obj_create(zend_class_entry *ce)
{
    struct php_perf_handle_obj *obj;

    obj = ecalloc(1, sizeof(*obj) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &php_perf_handle_obj_handlers;

    return &obj->std;
}

PERF_PUBLIC
void php_perf_handle_reset(struct php_perf_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        handle_ioctl_error();
    }
}

PERF_PUBLIC
void php_perf_handle_enable(struct php_perf_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        handle_ioctl_error();
    }
}

PERF_PUBLIC
void php_perf_handle_disable(struct php_perf_handle *handle)
{
    int err;

    if (UNEXPECTED(handle->metrics_count <= 0)) {
        return;
    }

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    if (UNEXPECTED(err == -1)) {
        handle_ioctl_error();
    }
}

PERF_PUBLIC
void php_perf_handle_close(struct php_perf_handle *handle)
{
    int err;

    for (ssize_t i = (ssize_t) handle->metrics_count - 1; i >= 0; i--) {
        err = close(handle->metrics[i].fd);
        if (err == -1) {
            php_error_docref(NULL, E_WARNING, "perf: close: %s", strerror(errno));
            // continue even if it fails
        }
        if (EXPECTED(handle->metrics[i].name)) {
            zend_string_release(handle->metrics[i].name);
        }
    }

    handle->metrics_count = 0;
}

PERF_PUBLIC
void php_perf_handle_read_to_array(struct php_perf_handle *handle, zval *return_value)
{
    php_perf_handle_disable(handle);

    array_init(return_value);

    size_t size = sizeof(struct php_perf_read_format) +
                  sizeof(((struct php_perf_read_format){0}).values[0]) * handle->metrics_count;
    struct php_perf_read_format *data = alloca(size);

    ssize_t bytes_read = read(handle->metrics[0].fd, (void *) data, size);

    if (bytes_read == -1) {
        php_error_docref(NULL, E_WARNING, "perf: failed to read: %s", strerror(errno));
        goto done;
    }

    for (size_t i = 0; i < data->nr; i++) {
        struct php_perf_metric *e = &handle->metrics[i];

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
                php_error_docref(
                    NULL, E_WARNING, "perf: counter truncation for %.*s", (int) e->name->len, e->name->val
                );
                continue;
            }

            add_assoc_long_ex(return_value, e->name->val, e->name->len, (zend_long) data->values[i].value);
        }
    }

done:
    php_perf_handle_enable(handle);
}

PERF_PUBLIC
PHP_PERF_ATTR_WARN_UNUSED_RESULT
struct php_perf_handle *php_perf_handle_open(const char **event_names, size_t event_names_length, bool persist)
{
    int fd;
    uint64_t id;
    int group_fd;
    int err;

    struct php_perf_handle *handle =
        pecalloc(sizeof(struct php_perf_handle) + sizeof(struct php_perf_metric) * (event_names_length + 1), 1, 1);
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
        fd = (int) perf_event_open(&attr, 0, -1, -1, 0);
        if (UNEXPECTED(fd == -1)) {
            php_error_docref(
                NULL, E_WARNING, "perf: perf_event_open failed for %s: %s", "PERF_COUNT_SW_DUMMY", strerror(errno)
            );
            goto cleanup;
        }
        err = ioctl(fd, PERF_EVENT_IOC_ID, &id);
        if (err == -1) {
        }
        ioctl(fd, PERF_EVENT_IOC_RESET, fd);
        handle->metrics[handle->metrics_count++] = (struct php_perf_metric){
            .fd = fd,
            .id = id,
            .name = zend_string_init(ZEND_STRL("PERF_COUNT_SW_DUMMY"), persist),
        };
        group_fd = fd;
    } while (false);

    // Open the other events
    for (size_t i = 0; i < event_names_length; i++) {
        const char *event_name = event_names[i];
        struct perf_event_attr attr = {0};
        pfm_perf_encode_arg_t arg = {0};
        arg.attr = &attr;
        arg.size = sizeof(arg);

        err = pfm_get_os_event_encoding(event_name, PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
        if (UNEXPECTED(err != PFM_SUCCESS)) {
            php_error_docref(
                NULL, E_WARNING, "perf: failed to get event encoding for %s: %s", event_name, pfm_strerror(err)
            );
            goto cleanup;
        }

        attr.size = sizeof(attr);
        attr.disabled = 0;
        attr.watermark = 0;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

        fd = (int) perf_event_open(&attr, 0, -1, group_fd, 0);

        if (UNEXPECTED(fd == -1)) {
            php_error_docref(NULL, E_WARNING, "perf: perf_event_open failed for %s: %s", event_name, strerror(errno));
            goto cleanup;
        }

        ioctl(fd, PERF_EVENT_IOC_ID, &id);
        ioctl(fd, PERF_EVENT_IOC_RESET, fd);

        ZEND_ASSERT(handle->metrics_count < handle->metrics_size);

        handle->metrics[handle->metrics_count++] = (struct php_perf_metric){
            .fd = fd,
            .id = id,
            .name = zend_string_init(event_name, strlen(event_name), persist),
        };
    }

    return handle;

cleanup:
    php_perf_handle_close(handle);
    return NULL;
}

static PHP_METHOD(PerfExtHandle, disable)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    php_perf_handle_disable(obj->handle);
}

static PHP_METHOD(PerfExtHandle, enable)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    php_perf_handle_enable(obj->handle);
}

static PHP_METHOD(PerfExtHandle, read)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    php_perf_handle_read_to_array(obj->handle, return_value);
}

static PHP_METHOD(PerfExtHandle, reset)
{
    zval *self = getThis();

    ZEND_PARSE_PARAMETERS_NONE();

    struct php_perf_handle_obj *obj = php_perf_fetch_handle_object(Z_OBJ_P(self));

    ZEND_ASSERT(obj->handle->marker == PHP_PERF_HANDLE_MARKER);

    php_perf_handle_reset(obj->handle);
}

static zend_function_entry php_perf_handle_methods[] = {
    PHP_ME(PerfExtHandle, disable, perf_handle_disable_arginfo, ZEND_ACC_PUBLIC)
        PHP_ME(PerfExtHandle, enable, perf_handle_enable_arginfo, ZEND_ACC_PUBLIC)
            PHP_ME(PerfExtHandle, read, perf_handle_read_arginfo, ZEND_ACC_PUBLIC)
                PHP_ME(PerfExtHandle, reset, perf_handle_reset_arginfo, ZEND_ACC_PUBLIC) PHP_FE_END};

PERF_LOCAL
zend_result php_perf_handle_minit(void)
{
    zend_class_entry ce;

    memcpy(&php_perf_handle_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    php_perf_handle_obj_handlers.offset = XtOffsetOf(struct php_perf_handle_obj, std);
    php_perf_handle_obj_handlers.free_obj = php_perf_handle_obj_free;
    php_perf_handle_obj_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(ce, "PerfExt\\Handle", php_perf_handle_methods);
    perf_handle_ce = zend_register_internal_class(&ce);
    perf_handle_ce->create_object = php_perf_handle_obj_create;

    return SUCCESS;
}
