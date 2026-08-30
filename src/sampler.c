/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main/php.h"
#include "Zend/zend_API.h"
#include "Zend/zend_enum.h"
#include "Zend/zend_exceptions.h"
#if PHP_VERSION_ID >= 80300
#include "Zend/zend_hrtime.h"
#else
#include "ext/standard/hrtime.h"
#endif

#include "php_perfidious.h"
#include "sampler.h"

PERFIDIOUS_LOCAL zend_class_entry *perfidious_metric_ce;
PERFIDIOUS_LOCAL zend_class_entry *perfidious_scope_ce;

static zend_class_entry *perfidious_sampler_ce;
static zend_class_entry *perfidious_sample_ce;
static zend_class_entry *perfidious_sample_delta_ce;

static zend_object_handlers perfidious_sampler_obj_handlers;
static zend_object_handlers perfidious_sample_obj_handlers;
static zend_object_handlers perfidious_sample_delta_obj_handlers;

struct perfidious_sample_identity
{
    uint32_t refcount;
};

struct perfidious_sampler_obj
{
    struct perfidious_platform_sampler *sampler;
    struct perfidious_sample_identity *identity;
    struct perfidious_sampler_snapshot origin;
    uint64_t time_origin_ns;
    uint32_t metrics;
    uint8_t metric_order[PERFIDIOUS_METRIC_COUNT];
    uint8_t metric_count;
    zend_object std;
};

struct perfidious_sample_obj
{
    struct perfidious_sample_identity *identity;
    uint64_t values[PERFIDIOUS_METRIC_COUNT];
    uint64_t time_ns;
    uint32_t metrics;
    zend_object std;
};

struct perfidious_sample_delta_obj
{
    uint64_t values[PERFIDIOUS_METRIC_COUNT];
    uint32_t metrics;
    zend_object std;
};

static zend_always_inline struct perfidious_sampler_obj *perfidious_fetch_sampler_object(zend_object *object)
{
    return (struct perfidious_sampler_obj *) ((char *) object - XtOffsetOf(struct perfidious_sampler_obj, std));
}

static zend_always_inline struct perfidious_sample_obj *perfidious_fetch_sample_object(zend_object *object)
{
    return (struct perfidious_sample_obj *) ((char *) object - XtOffsetOf(struct perfidious_sample_obj, std));
}

static zend_always_inline struct perfidious_sample_delta_obj *perfidious_fetch_sample_delta_object(zend_object *object)
{
    return (struct perfidious_sample_delta_obj *) ((char *) object -
                                                   XtOffsetOf(struct perfidious_sample_delta_obj, std));
}

static zend_always_inline uint64_t perfidious_hrtime_current(void)
{
#if PHP_VERSION_ID >= 80300
    return (uint64_t) zend_hrtime();
#else
    return (uint64_t) php_hrtime_current();
#endif
}

static void perfidious_sample_identity_release(struct perfidious_sample_identity *identity)
{
    if (identity != NULL && --identity->refcount == 0) {
        efree(identity);
    }
}

static void perfidious_sampler_obj_free(zend_object *object)
{
    struct perfidious_sampler_obj *obj = perfidious_fetch_sampler_object(object);

    if (obj->sampler != NULL) {
        perfidious_platform_sampler_close(obj->sampler);
        obj->sampler = NULL;
    }
    perfidious_sample_identity_release(obj->identity);
    obj->identity = NULL;
    zend_object_std_dtor(object);
}

static zend_object *perfidious_sampler_obj_create(zend_class_entry *class_entry)
{
    struct perfidious_sampler_obj *obj = zend_object_alloc(sizeof(*obj), class_entry);

    obj->sampler = NULL;
    obj->identity = NULL;
    memset(&obj->origin, 0, sizeof(obj->origin));
    obj->time_origin_ns = 0;
    obj->metrics = 0;
    memset(obj->metric_order, 0, sizeof(obj->metric_order));
    obj->metric_count = 0;
    zend_object_std_init(&obj->std, class_entry);
    object_properties_init(&obj->std, class_entry);
    obj->std.handlers = &perfidious_sampler_obj_handlers;

    return &obj->std;
}

static void perfidious_sample_obj_free(zend_object *object)
{
    struct perfidious_sample_obj *obj = perfidious_fetch_sample_object(object);

    perfidious_sample_identity_release(obj->identity);
    obj->identity = NULL;
    zend_object_std_dtor(object);
}

static zend_object *perfidious_sample_obj_create(zend_class_entry *class_entry)
{
    struct perfidious_sample_obj *obj = zend_object_alloc(sizeof(*obj), class_entry);

    obj->identity = NULL;
    memset(obj->values, 0, sizeof(obj->values));
    obj->time_ns = 0;
    obj->metrics = 0;
    zend_object_std_init(&obj->std, class_entry);
    object_properties_init(&obj->std, class_entry);
    obj->std.handlers = &perfidious_sample_obj_handlers;

    return &obj->std;
}

static zend_object *perfidious_sample_delta_obj_create(zend_class_entry *class_entry)
{
    struct perfidious_sample_delta_obj *obj = zend_object_alloc(sizeof(*obj), class_entry);

    memset(obj->values, 0, sizeof(obj->values));
    obj->metrics = 0;
    zend_object_std_init(&obj->std, class_entry);
    object_properties_init(&obj->std, class_entry);
    obj->std.handlers = &perfidious_sample_delta_obj_handlers;

    return &obj->std;
}

static void
perfidious_add_string_enum_case(zend_class_entry *class_entry, const char *name, const char *value, size_t value_length)
{
    zval backing_value;

    ZVAL_STR(&backing_value, zend_string_init_interned(value, value_length, true));
    zend_enum_add_case_cstr(class_entry, name, &backing_value);
}

#define PERFIDIOUS_ADD_STRING_ENUM_CASE(class_entry, name, value)                                                      \
    perfidious_add_string_enum_case(class_entry, name, ZEND_STRL(value))

static const char *perfidious_metric_name(enum perfidious_metric_id metric)
{
    switch (metric) {
        case PERFIDIOUS_METRIC_CPU_TIME:
            return "cpu-time";
        case PERFIDIOUS_METRIC_PAGE_FAULTS:
            return "page-faults";
        case PERFIDIOUS_METRIC_CONTEXT_SWITCHES:
            return "context-switches";
        case PERFIDIOUS_METRIC_CPU_CYCLES:
            return "cpu-cycles";
        case PERFIDIOUS_METRIC_INSTRUCTIONS:
            return "instructions";
        default:
        case PERFIDIOUS_METRIC_COUNT:
            return "unknown";
    }
}

static const char *perfidious_metric_case_name(enum perfidious_metric_id metric)
{
    switch (metric) {
        case PERFIDIOUS_METRIC_CPU_TIME:
            return "CpuTime";
        case PERFIDIOUS_METRIC_PAGE_FAULTS:
            return "PageFaults";
        case PERFIDIOUS_METRIC_CONTEXT_SWITCHES:
            return "ContextSwitches";
        case PERFIDIOUS_METRIC_CPU_CYCLES:
            return "CpuCycles";
        case PERFIDIOUS_METRIC_INSTRUCTIONS:
            return "Instructions";
        default:
        case PERFIDIOUS_METRIC_COUNT:
            return "";
    }
}

static bool perfidious_metric_from_zval(zval *value, enum perfidious_metric_id *metric)
{
    zval *backing_value;
    zend_string *name;

    ZVAL_DEREF(value);
    if (Z_TYPE_P(value) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(value), perfidious_metric_ce)) {
        zend_type_error("All metrics must be instances of Perfidious\\Metric");
        return false;
    }

    backing_value = zend_enum_fetch_case_value(Z_OBJ_P(value));
    ZEND_ASSERT(Z_TYPE_P(backing_value) == IS_STRING);
    name = Z_STR_P(backing_value);

    if (zend_string_equals_literal(name, "cpu-time")) {
        *metric = PERFIDIOUS_METRIC_CPU_TIME;
    } else if (zend_string_equals_literal(name, "page-faults")) {
        *metric = PERFIDIOUS_METRIC_PAGE_FAULTS;
    } else if (zend_string_equals_literal(name, "context-switches")) {
        *metric = PERFIDIOUS_METRIC_CONTEXT_SWITCHES;
    } else if (zend_string_equals_literal(name, "cpu-cycles")) {
        *metric = PERFIDIOUS_METRIC_CPU_CYCLES;
    } else if (zend_string_equals_literal(name, "instructions")) {
        *metric = PERFIDIOUS_METRIC_INSTRUCTIONS;
    } else {
        zend_throw_error(NULL, "Unknown Perfidious\\Metric backing value \"%s\"", ZSTR_VAL(name));
        return false;
    }

    return true;
}

static enum perfidious_scope_id perfidious_scope_from_zval(zval *value)
{
    if (value == NULL || Z_OBJ_P(value) == zend_enum_get_case_cstr(perfidious_scope_ce, "CurrentProcess")) {
        return PERFIDIOUS_SCOPE_CURRENT_PROCESS;
    }

    ZEND_ASSERT(Z_OBJ_P(value) == zend_enum_get_case_cstr(perfidious_scope_ce, "CurrentThread"));
    return PERFIDIOUS_SCOPE_CURRENT_THREAD;
}

static bool perfidious_uint64_to_long(uint64_t value, zend_long *result)
{
    if (UNEXPECTED(value > (uint64_t) ZEND_LONG_MAX)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce, 0, "Sampler counter value is too large to represent as a PHP integer"
        );
        return false;
    }

    *result = (zend_long) value;
    return true;
}

static void perfidious_throw_unsupported_metrics(
    enum perfidious_scope_id scope, const uint8_t *metric_order, uint8_t metric_count, uint32_t rejected
)
{
    char names[128];
    size_t used = 0;
    zend_string *message;
    zval message_value;
    zval code_value;
    zval exception;
    zval scope_value;
    zval unsupported_metrics;

    names[0] = '\0';
    for (uint8_t i = 0; i < metric_count; i++) {
        enum perfidious_metric_id metric = (enum perfidious_metric_id) metric_order[i];
        int written;

        if ((rejected & PERFIDIOUS_METRIC_MASK(metric)) == 0) {
            continue;
        }

        written =
            snprintf(names + used, sizeof(names) - used, "%s%s", used == 0 ? "" : ", ", perfidious_metric_name(metric));
        if (written < 0 || (size_t) written >= sizeof(names) - used) {
            break;
        }
        used += (size_t) written;
    }

    message = strpprintf(
        0,
        "Metrics [%s] are not supported for scope %s",
        names,
        scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS ? "current-process" : "current-thread"
    );

    object_init_ex(&exception, perfidious_unsupported_metric_exception_ce);
    ZVAL_STR(&message_value, message);
    ZVAL_LONG(&code_value, 0);
    zend_call_known_instance_method_with_2_params(
        zend_ce_exception->constructor, Z_OBJ(exception), NULL, &message_value, &code_value
    );
    zval_ptr_dtor(&message_value);
    if (UNEXPECTED(EG(exception) != NULL)) {
        zval_ptr_dtor(&exception);
        return;
    }

    ZVAL_OBJ_COPY(
        &scope_value,
        zend_enum_get_case_cstr(
            perfidious_scope_ce, scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS ? "CurrentProcess" : "CurrentThread"
        )
    );
    array_init_size(&unsupported_metrics, metric_count);
    for (uint8_t i = 0; i < metric_count; i++) {
        enum perfidious_metric_id metric = (enum perfidious_metric_id) metric_order[i];
        zval metric_value;

        if ((rejected & PERFIDIOUS_METRIC_MASK(metric)) == 0) {
            continue;
        }

        ZVAL_OBJ_COPY(
            &metric_value, zend_enum_get_case_cstr(perfidious_metric_ce, perfidious_metric_case_name(metric))
        );
        add_next_index_zval(&unsupported_metrics, &metric_value);
    }

    zend_update_property(
        perfidious_unsupported_metric_exception_ce, Z_OBJ(exception), ZEND_STRL("scope"), &scope_value
    );
    zend_update_property(
        perfidious_unsupported_metric_exception_ce,
        Z_OBJ(exception),
        ZEND_STRL("unsupportedMetrics"),
        &unsupported_metrics
    );
    zval_ptr_dtor(&scope_value);
    zval_ptr_dtor(&unsupported_metrics);

    zend_throw_exception_object(&exception);
}

// clang-format off
ZEND_BEGIN_ARG_INFO_EX(perfidious_private_construct_arginfo, false, 0, 0)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousSamplerObject, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_sampler_open_arginfo, false, 1, Perfidious\\Sampler, false)
    ZEND_ARG_TYPE_INFO(false, metrics, IS_ARRAY, false)
    ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(false, scope, Perfidious\\Scope, false, "Perfidious\\Scope::CurrentProcess")
ZEND_END_ARG_INFO()
// clang-format on

static PHP_METHOD(PerfidiousSampler, open)
{
    HashTable *metrics_array;
    zval *scope_value = NULL;
    zval *value;
    enum perfidious_scope_id scope;
    uint32_t metrics = 0;
    uint32_t supported;
    uint8_t metric_order[PERFIDIOUS_METRIC_COUNT];
    uint8_t metric_count = 0;
    struct perfidious_platform_sampler *platform_sampler = NULL;
    struct perfidious_sampler_snapshot origin;
    struct perfidious_sampler_obj *obj;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY_HT(metrics_array)
        Z_PARAM_OPTIONAL
        Z_PARAM_OBJECT_OF_CLASS(scope_value, perfidious_scope_ce)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(zend_hash_num_elements(metrics_array) == 0)) {
        zend_value_error("Perfidious\\Sampler::open(): Argument #1 ($metrics) must not be empty");
        return;
    }

    ZEND_HASH_FOREACH_VAL(metrics_array, value)
    {
        enum perfidious_metric_id metric;
        uint32_t mask;

        if (!perfidious_metric_from_zval(value, &metric)) {
            return;
        }

        mask = PERFIDIOUS_METRIC_MASK(metric);
        if (UNEXPECTED((metrics & mask) != 0)) {
            zend_value_error("Perfidious\\Sampler::open(): Argument #1 ($metrics) must not contain duplicate metrics");
            return;
        }

        metrics |= mask;
        metric_order[metric_count++] = (uint8_t) metric;
    }
    ZEND_HASH_FOREACH_END();

    scope = perfidious_scope_from_zval(scope_value);
    if (UNEXPECTED(FAILURE == perfidious_platform_sampler_supported_metrics(metrics, scope, &supported))) {
        return;
    }
    if (UNEXPECTED((metrics & ~supported) != 0)) {
        perfidious_throw_unsupported_metrics(scope, metric_order, metric_count, metrics & ~supported);
        return;
    }

    if (UNEXPECTED(FAILURE == perfidious_platform_sampler_open(metrics, scope, &platform_sampler))) {
        return;
    }
    if (UNEXPECTED(FAILURE == perfidious_platform_sampler_read(platform_sampler, &origin))) {
        perfidious_platform_sampler_close(platform_sampler);
        return;
    }

    object_init_ex(return_value, perfidious_sampler_ce);
    obj = perfidious_fetch_sampler_object(Z_OBJ_P(return_value));
    obj->sampler = platform_sampler;
    obj->origin = origin;
    obj->time_origin_ns = perfidious_hrtime_current();
    obj->metrics = metrics;
    memcpy(obj->metric_order, metric_order, metric_count);
    obj->metric_count = metric_count;
    obj->identity = emalloc(sizeof(*obj->identity));
    obj->identity->refcount = 1;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(perfidious_sampler_metrics_arginfo, false, 0, IS_ARRAY, false)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousSampler, metrics)
{
    struct perfidious_sampler_obj *obj;

    ZEND_PARSE_PARAMETERS_NONE();

    obj = perfidious_fetch_sampler_object(Z_OBJ_P(ZEND_THIS));
    array_init_size(return_value, obj->metric_count);
    for (uint8_t i = 0; i < obj->metric_count; i++) {
        zend_object *case_object = zend_enum_get_case_cstr(
            perfidious_metric_ce, perfidious_metric_case_name((enum perfidious_metric_id) obj->metric_order[i])
        );
        zval metric;

        ZVAL_OBJ_COPY(&metric, case_object);
        add_next_index_zval(return_value, &metric);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_sampler_read_arginfo, false, 0, Perfidious\\Sample, false)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousSampler, read)
{
    struct perfidious_sampler_obj *obj;
    struct perfidious_sample_obj *sample;
    struct perfidious_sampler_snapshot current;
    uint64_t time_ns;

    ZEND_PARSE_PARAMETERS_NONE();

    obj = perfidious_fetch_sampler_object(Z_OBJ_P(ZEND_THIS));
    if (UNEXPECTED(obj->sampler == NULL)) {
        zend_throw_exception(perfidious_io_exception_ce, "Sampler is closed", 0);
        return;
    }

    if (UNEXPECTED(FAILURE == perfidious_platform_sampler_read(obj->sampler, &current))) {
        return;
    }
    time_ns = perfidious_hrtime_current();

    if (UNEXPECTED(time_ns < obj->time_origin_ns)) {
        zend_throw_exception(perfidious_io_exception_ce, "Monotonic clock moved backwards", 0);
        return;
    }

    object_init_ex(return_value, perfidious_sample_ce);
    sample = perfidious_fetch_sample_object(Z_OBJ_P(return_value));
    sample->identity = obj->identity;
    sample->identity->refcount++;
    sample->metrics = obj->metrics;
    sample->time_ns = time_ns - obj->time_origin_ns;

    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        if ((obj->metrics & PERFIDIOUS_METRIC_MASK(metric)) == 0) {
            continue;
        }
        if (UNEXPECTED(current.values[metric] < obj->origin.values[metric])) {
            zval_ptr_dtor(return_value);
            ZVAL_UNDEF(return_value);
            zend_throw_exception_ex(
                perfidious_io_exception_ce, 0, "Sampler %s counter moved backwards", perfidious_metric_name(metric)
            );
            return;
        }
        sample->values[metric] = current.values[metric] - obj->origin.values[metric];
        if (UNEXPECTED(sample->values[metric] > (uint64_t) ZEND_LONG_MAX)) {
            zval_ptr_dtor(return_value);
            ZVAL_UNDEF(return_value);
            zend_throw_exception_ex(
                perfidious_overflow_exception_ce,
                0,
                "Sampler %s counter is too large to represent as a PHP integer",
                perfidious_metric_name(metric)
            );
            return;
        }
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(perfidious_sampler_close_arginfo, false, 0, IS_VOID, false)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousSampler, close)
{
    struct perfidious_sampler_obj *obj;

    ZEND_PARSE_PARAMETERS_NONE();

    obj = perfidious_fetch_sampler_object(Z_OBJ_P(ZEND_THIS));
    if (obj->sampler != NULL) {
        perfidious_platform_sampler_close(obj->sampler);
        obj->sampler = NULL;
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(perfidious_sample_value_arginfo, false, 1, IS_LONG, false)
    ZEND_ARG_OBJ_INFO(false, metric, Perfidious\\Metric, false)
ZEND_END_ARG_INFO()

static void perfidious_sample_value(INTERNAL_FUNCTION_PARAMETERS, uint32_t metrics, const uint64_t *values)
{
    zval *metric_value;
    enum perfidious_metric_id metric;
    zend_long result;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(metric_value, perfidious_metric_ce)
    ZEND_PARSE_PARAMETERS_END();

    if (!perfidious_metric_from_zval(metric_value, &metric)) {
        return;
    }
    if (UNEXPECTED((metrics & PERFIDIOUS_METRIC_MASK(metric)) == 0)) {
        zend_value_error("Metric %s was not collected by this sampler", perfidious_metric_name(metric));
        return;
    }
    if (!perfidious_uint64_to_long(values[metric], &result)) {
        return;
    }

    RETURN_LONG(result);
}

static PHP_METHOD(PerfidiousSample, value)
{
    struct perfidious_sample_obj *obj = perfidious_fetch_sample_object(Z_OBJ_P(ZEND_THIS));

    perfidious_sample_value(INTERNAL_FUNCTION_PARAM_PASSTHRU, obj->metrics, obj->values);
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_sample_since_arginfo, false, 1, Perfidious\\SampleDelta, false)
    ZEND_ARG_OBJ_INFO(false, earlier, Perfidious\\Sample, false)
ZEND_END_ARG_INFO()

static PHP_METHOD(PerfidiousSample, since)
{
    zval *earlier_value;
    struct perfidious_sample_obj *obj;
    struct perfidious_sample_obj *earlier;
    struct perfidious_sample_delta_obj *delta;
    zend_long elapsed_time_ns;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(earlier_value, perfidious_sample_ce)
    ZEND_PARSE_PARAMETERS_END();

    obj = perfidious_fetch_sample_object(Z_OBJ_P(ZEND_THIS));
    earlier = perfidious_fetch_sample_object(Z_OBJ_P(earlier_value));
    if (UNEXPECTED(obj->identity != earlier->identity)) {
        zend_value_error("Samples must come from the same sampler");
        return;
    }
    if (UNEXPECTED(obj->time_ns < earlier->time_ns)) {
        zend_value_error("The earlier sample must not be newer than this sample");
        return;
    }
    if (!perfidious_uint64_to_long(obj->time_ns - earlier->time_ns, &elapsed_time_ns)) {
        return;
    }

    object_init_ex(return_value, perfidious_sample_delta_ce);
    delta = perfidious_fetch_sample_delta_object(Z_OBJ_P(return_value));
    delta->metrics = obj->metrics;

    for (enum perfidious_metric_id metric = 0; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        if ((obj->metrics & PERFIDIOUS_METRIC_MASK(metric)) == 0) {
            continue;
        }
        if (UNEXPECTED(obj->values[metric] < earlier->values[metric])) {
            zval_ptr_dtor(return_value);
            ZVAL_UNDEF(return_value);
            zend_throw_exception_ex(
                perfidious_io_exception_ce,
                0,
                "Sampler %s counter moved backwards between samples",
                perfidious_metric_name(metric)
            );
            return;
        }
        delta->values[metric] = obj->values[metric] - earlier->values[metric];
    }

    zend_update_property_long(
        perfidious_sample_delta_ce, Z_OBJ_P(return_value), ZEND_STRL("elapsedTimeNs"), elapsed_time_ns
    );
}

static PHP_METHOD(PerfidiousSampleDelta, value)
{
    struct perfidious_sample_delta_obj *obj = perfidious_fetch_sample_delta_object(Z_OBJ_P(ZEND_THIS));

    perfidious_sample_value(INTERNAL_FUNCTION_PARAM_PASSTHRU, obj->metrics, obj->values);
}

// clang-format off
static const zend_function_entry perfidious_sampler_methods[] = {
    PHP_ME(PerfidiousSamplerObject, __construct, perfidious_private_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_ME(PerfidiousSampler, open, perfidious_sampler_open_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(PerfidiousSampler, metrics, perfidious_sampler_metrics_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(PerfidiousSampler, read, perfidious_sampler_read_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(PerfidiousSampler, close, perfidious_sampler_close_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry perfidious_sample_methods[] = {
    PHP_ME(PerfidiousSamplerObject, __construct, perfidious_private_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_ME(PerfidiousSample, value, perfidious_sample_value_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(PerfidiousSample, since, perfidious_sample_since_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry perfidious_sample_delta_methods[] = {
    PHP_ME(PerfidiousSamplerObject, __construct, perfidious_private_construct_arginfo, ZEND_ACC_PRIVATE)
    PHP_ME(PerfidiousSampleDelta, value, perfidious_sample_value_arginfo, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
// clang-format on

static void
perfidious_declare_readonly_long_property(zend_class_entry *class_entry, const char *name, size_t name_length)
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

PERFIDIOUS_LOCAL void perfidious_sampler_minit(void)
{
    zend_class_entry class_entry;

    perfidious_metric_ce = zend_register_internal_enum(PHP_PERFIDIOUS_NAMESPACE "\\Metric", IS_STRING, NULL);
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_metric_ce, "CpuTime", "cpu-time");
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_metric_ce, "PageFaults", "page-faults");
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_metric_ce, "ContextSwitches", "context-switches");
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_metric_ce, "CpuCycles", "cpu-cycles");
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_metric_ce, "Instructions", "instructions");

    perfidious_scope_ce = zend_register_internal_enum(PHP_PERFIDIOUS_NAMESPACE "\\Scope", IS_STRING, NULL);
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_scope_ce, "CurrentProcess", "current-process");
    PERFIDIOUS_ADD_STRING_ENUM_CASE(perfidious_scope_ce, "CurrentThread", "current-thread");

    INIT_CLASS_ENTRY(class_entry, PHP_PERFIDIOUS_NAMESPACE "\\Sampler", perfidious_sampler_methods);
    perfidious_sampler_ce = zend_register_internal_class(&class_entry);
    perfidious_sampler_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    perfidious_sampler_ce->create_object = perfidious_sampler_obj_create;
    memcpy(&perfidious_sampler_obj_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    perfidious_sampler_obj_handlers.offset = XtOffsetOf(struct perfidious_sampler_obj, std);
    perfidious_sampler_obj_handlers.free_obj = perfidious_sampler_obj_free;
    perfidious_sampler_obj_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(class_entry, PHP_PERFIDIOUS_NAMESPACE "\\Sample", perfidious_sample_methods);
    perfidious_sample_ce = zend_register_internal_class(&class_entry);
    perfidious_sample_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    perfidious_sample_ce->create_object = perfidious_sample_obj_create;
    memcpy(&perfidious_sample_obj_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    perfidious_sample_obj_handlers.offset = XtOffsetOf(struct perfidious_sample_obj, std);
    perfidious_sample_obj_handlers.free_obj = perfidious_sample_obj_free;
    perfidious_sample_obj_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(class_entry, PHP_PERFIDIOUS_NAMESPACE "\\SampleDelta", perfidious_sample_delta_methods);
    perfidious_sample_delta_ce = zend_register_internal_class(&class_entry);
    perfidious_sample_delta_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
    perfidious_sample_delta_ce->create_object = perfidious_sample_delta_obj_create;
    memcpy(&perfidious_sample_delta_obj_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    perfidious_sample_delta_obj_handlers.offset = XtOffsetOf(struct perfidious_sample_delta_obj, std);
    perfidious_sample_delta_obj_handlers.clone_obj = NULL;
    perfidious_declare_readonly_long_property(perfidious_sample_delta_ce, ZEND_STRL("elapsedTimeNs"));
}
