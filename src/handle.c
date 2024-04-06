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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <linux/perf_event.h>
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

#include "php_perfidious.h"
#include "handle.h"
#include "private.h"

PERFIDIOUS_LOCAL zend_string *PERFIDIOUS_INTERNED_PERF_COUNT_SW_DUMMY;
PERFIDIOUS_PUBLIC zend_class_entry *perfidious_handle_ce;
static zend_object_handlers perfidious_handle_obj_handlers;

static void perfidious_handle_ioctl_error(void)
{
    perfidious_error_helper(perfidious_io_exception_ce, errno, "ioctl failed: %s", strerror(errno));
}

#define HANDLE_IOCTL_ERROR(err)                                                                                        \
    do {                                                                                                               \
        if (UNEXPECTED(err == -1)) {                                                                                   \
            perfidious_handle_ioctl_error();                                                                           \
            return FAILURE;                                                                                            \
        }                                                                                                              \
    } while (false)

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
zend_result perfidious_handle_reset(struct perfidious_handle *restrict handle)
{
    int err;

    PERFIDIOUS_ASSERT_RETURN(handle->metrics_count > 0);

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    HANDLE_IOCTL_ERROR(err);

    return SUCCESS;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_enable(struct perfidious_handle *restrict handle)
{
    int err;

    PERFIDIOUS_ASSERT_RETURN(handle->metrics_count > 0);

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    HANDLE_IOCTL_ERROR(err);

    handle->enabled = true;

    return SUCCESS;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_disable(struct perfidious_handle *restrict handle)
{
    int err;

    PERFIDIOUS_ASSERT_RETURN(handle->metrics_count > 0);

    err = ioctl(handle->metrics[0].fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    HANDLE_IOCTL_ERROR(err);

    handle->enabled = false;

    return SUCCESS;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_close(struct perfidious_handle *restrict handle)
{
    zend_result rv = SUCCESS;

    for (ssize_t i = (ssize_t) handle->metrics_count - 1; i >= 0; i--) {
        if (UNEXPECTED(-1 == close(handle->metrics[i].fd))) {
            zend_throw_exception_ex(perfidious_io_exception_ce, errno, "close failed: %s", strerror(errno));
            rv = FAILURE;
            // continue even if it fails
        }
        if (EXPECTED(handle->metrics[i].name)) {
            zend_string_release(handle->metrics[i].name);
        }
    }

    handle->metrics_count = 0;

    return rv;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
size_t perfidious_handle_read_buffer_size(struct perfidious_handle *restrict handle)
{
    return sizeof(struct perfidious_read_format) +
           sizeof(((struct perfidious_read_format){0}).values[0]) * handle->metrics_count;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_raw(struct perfidious_handle *restrict handle, size_t size, void *restrict buffer)
{
    ssize_t bytes_read = read(handle->metrics[0].fd, buffer, size);

    PERFIDIOUS_ASSERT_RETURN(bytes_read == (ssize_t) size);

    return SUCCESS;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_to_array_with_times(
    struct perfidious_handle *restrict handle,
    zval *restrict return_value,
    uint64_t *restrict time_enabled,
    uint64_t *restrict time_running
)
{
    size_t size = perfidious_handle_read_buffer_size(handle);
    struct perfidious_read_format *data = alloca(size);

    if (SUCCESS != perfidious_handle_read_raw(handle, size, data)) {
        perfidious_error_helper(perfidious_io_exception_ce, errno, "failed to read: %s", strerror(errno));
        return FAILURE;
    }

    *time_enabled = data->time_enabled;
    *time_running = data->time_running;

    array_init(return_value);

    for (size_t i = 0; i < data->nr; i++) {
        struct perfidious_metric *metric = &handle->metrics[i];
        struct perfidious_read_format_value *value = &data->values[i];

        if (UNEXPECTED(metric->id != value->id)) {
            metric = NULL;

            // skip the first entry - it should be the dummy
            for (size_t j = 1; j < handle->metrics_count; j++) {
                if (handle->metrics[j].id == value->id) {
                    metric = &handle->metrics[j];
                    break;
                }
            }

            if (UNEXPECTED(metric == NULL)) {
                continue;
            }
        } else if (i == 0) {
            // skip the first entry - it should be the dummy
            continue;
        }

        if (EXPECTED(metric->name != NULL)) {
            zend_long value_zl = 0;
            zval tmp = {0};

            PERFIDIOUS_ASSERT_RETURN_EX(perfidious_uint64_t_to_zend_long(value->value, &value_zl), {
                zval_ptr_dtor(return_value);
                ZVAL_UNDEF(return_value);
            });

            ZVAL_LONG(&tmp, value_zl);
            zend_symtable_update(Z_ARRVAL_P(return_value), metric->name, &tmp);
        }
    }

    return SUCCESS;
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_handle_read_to_array(struct perfidious_handle *handle, zval *return_value)
{
    uint64_t time_enabled;
    uint64_t time_running;
    return perfidious_handle_read_to_array_with_times(handle, return_value, &time_enabled, &time_running);
}

ZEND_HOT
PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
zend_result perfidious_handle_read_to_result(struct perfidious_handle *handle, zval *return_value)
{
    uint64_t time_enabled;
    uint64_t time_running;
    zval arr = {0};
    zval tmp = {0};

    zend_result err = perfidious_handle_read_to_array_with_times(handle, &arr, &time_enabled, &time_running);
    PERFIDIOUS_ASSERT_RETURN(err == SUCCESS);

    zend_long time_enabled_zl = 0;
    PERFIDIOUS_ASSERT_RETURN_EX(perfidious_uint64_t_to_zend_long(time_enabled, &time_enabled_zl), {
        zval_ptr_dtor(&arr);
    });

    zend_long time_running_zl = 0;
    PERFIDIOUS_ASSERT_RETURN_EX(perfidious_uint64_t_to_zend_long(time_running, &time_running_zl), {
        zval_ptr_dtor(&arr);
    });

    object_init_ex(return_value, perfidious_read_result_ce);

    ZVAL_LONG(&tmp, time_enabled_zl);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_TIME_ENABLED, &tmp);

    ZVAL_LONG(&tmp, time_running_zl);
    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_TIME_RUNNING, &tmp);

    zend_update_property_ex(Z_OBJCE_P(return_value), Z_OBJ_P(return_value), PERFIDIOUS_INTERNED_VALUES, &arr);

    zval_ptr_dtor(&arr); // fear

    return SUCCESS;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *
perfidious_handle_open(zend_string **restrict event_names, size_t event_names_length, bool persist)
{
    return perfidious_handle_open_ex(event_names, event_names_length, 0, -1, persist);
}

static inline zend_result perfidious_perf_event_open(
    struct perf_event_attr *restrict attr,
    const char *restrict event_name,
    size_t event_name_length,
    pid_t pid,
    int cpu,
    int group_fd,
    int *restrict fd_out,
    uint64_t *restrict id_out
)
{
    int id;
    int fd = (int) perf_event_open(attr, pid, cpu, group_fd, 0);
    if (UNEXPECTED(fd == -1)) {
        perfidious_error_helper(
            perfidious_io_exception_ce,
            errno,
            "perf_event_open() failed for %.*s: %s",
            (int) event_name_length,
            event_name,
            strerror(errno)
        );
        return FAILURE;
    }

    if (UNEXPECTED(-1 == ioctl(fd, PERF_EVENT_IOC_ID, &id))) {
        perfidious_handle_ioctl_error();
        close(fd);
        return FAILURE;
    }

    if (UNEXPECTED(-1 == ioctl(fd, PERF_EVENT_IOC_RESET, fd))) {
        perfidious_handle_ioctl_error();
        close(fd);
        return FAILURE;
    }

    *fd_out = fd;
    *id_out = id;

    return SUCCESS;
}

PERFIDIOUS_PUBLIC
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
struct perfidious_handle *perfidious_handle_open_ex(
    zend_string **restrict event_names, size_t event_names_length, pid_t pid, int cpu, bool persist
)
{
    int fd;
    uint64_t id;
    int group_fd;
    zend_result err;
    uint64_t format =
        PERF_FORMAT_GROUP | PERF_FORMAT_ID | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_TOTAL_TIME_ENABLED;

    struct perfidious_handle *handle = pecalloc(
        sizeof(struct perfidious_handle) + sizeof(struct perfidious_metric) * (event_names_length + 1), 1, persist
    );
    handle->marker = PERFIDIOUS_HANDLE_MARKER;
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
            .read_format = format,
        };
        err = perfidious_perf_event_open(&attr, ZEND_STRL("perf::PERF_COUNT_SW_DUMMY"), pid, cpu, -1, &fd, &id);
        if (UNEXPECTED(FAILURE == err)) {
            goto cleanup;
        }
        handle->metrics[handle->metrics_count++] = (struct perfidious_metric){
            .fd = fd,
            .id = id,
            .name = PERFIDIOUS_INTERNED_PERF_COUNT_SW_DUMMY,
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

        pfm_err_t pfm_err = pfm_get_os_event_encoding(ZSTR_VAL(event_name), PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
        if (UNEXPECTED(pfm_err != PFM_SUCCESS)) {
            zend_throw_exception_ex(
                perfidious_pmu_event_not_found_exception_ce,
                pfm_err,
                "failed to get libpfm event encoding for %.*s: %s",
                (int) ZSTR_LEN(event_name),
                ZSTR_VAL(event_name),
                pfm_strerror(pfm_err)
            );
            goto cleanup;
        }

        attr.size = sizeof(attr);
        attr.disabled = 0;
        attr.watermark = 0;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.read_format = format;

        err =
            perfidious_perf_event_open(&attr, ZSTR_VAL(event_name), ZSTR_LEN(event_name), pid, cpu, group_fd, &fd, &id);
        if (UNEXPECTED(FAILURE == err)) {
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
    pefree(handle, persist);
    return NULL;
}

ZEND_COLD
static PHP_METHOD(PerfidousHandle, disable)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    perfidious_handle_disable(obj->handle);

    RETURN_ZVAL(ZEND_THIS, 1, 0);
}

ZEND_COLD
static PHP_METHOD(PerfidousHandle, enable)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    perfidious_handle_enable(obj->handle);

    RETURN_ZVAL(ZEND_THIS, 1, 0);
}

ZEND_COLD
static PHP_METHOD(PerfidousHandle, rawStream)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    php_stream *stream = php_stream_fopen_from_fd(obj->handle->metrics[0].fd, "r", NULL);
    php_stream_to_zval(stream, return_value);
}

ZEND_HOT
static PHP_METHOD(PerfidousHandle, read)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    bool orig_enabled = obj->handle->enabled;

    if (orig_enabled) {
        perfidious_handle_disable(obj->handle);
    }

    perfidious_handle_read_to_result(obj->handle, return_value);

    if (orig_enabled) {
        perfidious_handle_enable(obj->handle);
    }
}

ZEND_HOT
static PHP_METHOD(PerfidousHandle, readArray)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    bool orig_enabled = obj->handle->enabled;

    if (orig_enabled) {
        perfidious_handle_disable(obj->handle);
    }

    if (UNEXPECTED(FAILURE == perfidious_handle_read_to_array(obj->handle, return_value))) {
        RETURN_NULL();
    }

    if (orig_enabled) {
        perfidious_handle_enable(obj->handle);
    }
}

ZEND_COLD
static PHP_METHOD(PerfidousHandle, reset)
{
    ZEND_PARSE_PARAMETERS_NONE();

    struct perfidious_handle_obj *obj = perfidious_fetch_handle_object(Z_OBJ_P(ZEND_THIS));

    if (UNEXPECTED(FAILURE == perfidious_handle_marker_assert(obj->handle))) {
        RETURN_NULL();
    }

    perfidious_handle_reset(obj->handle);

    RETURN_ZVAL(ZEND_THIS, 1, 0);
}

// clang-format off
static zend_function_entry perfidious_handle_methods[] = {
    PHP_ME(PerfidousHandle, disable, perfidious_handle_disable_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidousHandle, enable, perfidious_handle_enable_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidousHandle, rawStream, perfidious_handle_raw_stream_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidousHandle, read, perfidious_handle_read_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidousHandle, readArray, perfidious_handle_read_array_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(PerfidousHandle, reset, perfidious_handle_reset_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
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

    INIT_CLASS_ENTRY(ce, PHP_PERFIDIOUS_NAMESPACE "\\Handle", perfidious_handle_methods);
    class_entry = zend_register_internal_class(&ce);

    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    class_entry->create_object = perfidious_handle_obj_create;

    return class_entry;
}

PERFIDIOUS_LOCAL
void perfidious_handle_minit(void)
{
    PERFIDIOUS_INTERNED_PERF_COUNT_SW_DUMMY = zend_string_init_interned(ZEND_STRL("perf::PERF_COUNT_SW_DUMMY"), 1);

    perfidious_handle_ce = register_class_Handle();
}
