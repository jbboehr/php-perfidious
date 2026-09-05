/* Exercise the real sampler factory with bounded, test-only native resources. */
#include "../../src/sampler.c"

void perfidious_exceptions_minit(void);

struct perfidious_platform_sampler
{
    uint64_t value;
};

static zend_long live_resources;

static bool has_owner(struct perfidious_platform_sampler *sampler)
{
    zend_objects_store *store = &EG(objects_store);

    for (uint32_t i = 1; i < store->top; i++) {
        zend_object *object = store->object_buckets[i];
        if (IS_OBJ_VALID(object) && object->ce == perfidious_sampler_ce &&
            perfidious_fetch_sampler_object(object)->sampler == sampler) {
            return true;
        }
    }
    return false;
}

static bool fail_at(const char *stage)
{
    const char *setting = getenv("PERFIDIOUS_TEST_CONSTRUCTION_FAILURE");
    return setting != NULL && strcmp(setting, stage) == 0;
}

zend_result perfidious_platform_sampler_supported_metrics(
    uint32_t metrics, enum perfidious_scope_id scope, uint32_t *supported
)
{
    *supported = PERFIDIOUS_METRIC_CPU_TIME_MASK;
    return SUCCESS;
}

zend_result perfidious_platform_sampler_open(
    uint32_t metrics, enum perfidious_scope_id scope, struct perfidious_platform_sampler **sampler
)
{
    if (!has_owner(NULL)) {
        zend_throw_exception(perfidious_io_exception_ce, "No empty owner before native acquisition", 0);
        return FAILURE;
    }
    if (fail_at("open")) {
        zend_throw_exception(perfidious_io_exception_ce, "Native open failed", 0);
        return FAILURE;
    }
    *sampler = ecalloc(1, sizeof(**sampler));
    live_resources++;
    return SUCCESS;
}

zend_result perfidious_platform_sampler_read(
    struct perfidious_platform_sampler *sampler, struct perfidious_sampler_snapshot *snapshot
)
{
    if (!has_owner(sampler)) {
        zend_throw_exception(perfidious_io_exception_ce, "Native resource has no owner before read", 0);
        return FAILURE;
    }
    if (fail_at("read")) {
        zend_throw_exception(perfidious_io_exception_ce, "Native read failed", 0);
        return FAILURE;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->values[PERFIDIOUS_METRIC_CPU_TIME] = sampler->value++;
    return SUCCESS;
}

void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler)
{
    live_resources--;
    efree(sampler);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_live_arginfo, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_live)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(live_resources);
}

static const zend_function_entry fixture_functions[] = {
    PHP_FE(fixture_live, fixture_live_arginfo)
    PHP_FE_END
};

static PHP_MINIT_FUNCTION(fixture)
{
    perfidious_exceptions_minit();
    perfidious_sampler_minit();
    return SUCCESS;
}

static zend_module_entry fixture_module_entry = {
    STANDARD_MODULE_HEADER,
    "perfidious_construction_fixture",
    fixture_functions,
    PHP_MINIT(fixture),
    NULL, NULL, NULL, NULL,
    "1.0",
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(fixture)
