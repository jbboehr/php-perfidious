/**
 * Test-only standalone harness for the Darwin sampler's dynamic capability probe.
 *
 * Compile from the repository root with:
 *
 *   cc -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror \
 *      $(php-config --includes) -I. -Itests/darwin/shim \
 *      tests/darwin/sampler-probe-harness.c \
 *      -o /tmp/perfidious-darwin-sampler-probe
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"

#include "../../php_perfidious.h"
#include "../../src/darwin/resource_usage.h"
#include "../../src/sampler.h"
#include "libproc.h"

#undef ecalloc
#define ecalloc(nmemb, size) calloc((nmemb), (size))
#undef efree
#define efree(ptr) free(ptr)
#undef snprintf
#undef vsnprintf

static zend_class_entry io_exception_class;
static zend_class_entry overflow_exception_class;
static zend_class_entry wrong_thread_exception_class;

zend_class_entry *perfidious_io_exception_ce = &io_exception_class;
zend_class_entry *perfidious_overflow_exception_ce = &overflow_exception_class;
zend_class_entry *perfidious_wrong_thread_exception_ce = &wrong_thread_exception_class;

static zend_class_entry *thrown_class;
static zend_long thrown_code;
static char thrown_message[256];

static uint64_t proc_cycles[8];
static uint64_t proc_user_times[8];
static uint64_t proc_system_times[8];
static int proc_results[8];
static int proc_errors[8];
static size_t proc_sequence_length;
static size_t proc_call_count;

static uint64_t thread_ids[8];
static int thread_id_results[8];
static int thread_id_errors[8];
static size_t thread_id_sequence_length;
static size_t thread_id_call_count;
static struct perfidious_darwin_thread_resource_usage thread_usages[8];
static int thread_usage_results[8];
static int thread_usage_errors[8];
static size_t thread_usage_sequence_length;
static size_t thread_usage_call_count;

static int failure_count;

static void reset_exception(void)
{
    thrown_class = NULL;
    thrown_code = 0;
    thrown_message[0] = '\0';
}

static void
configure_proc_sequence(const uint64_t *cycles, const int *results, const int *errors, size_t sequence_length)
{
    if (sequence_length > sizeof(proc_cycles) / sizeof(proc_cycles[0])) {
        fprintf(stderr, "proc sequence is too long\n");
        exit(EXIT_FAILURE);
    }

    memset(proc_cycles, 0, sizeof(proc_cycles));
    memset(proc_user_times, 0, sizeof(proc_user_times));
    memset(proc_system_times, 0, sizeof(proc_system_times));
    memset(proc_results, 0, sizeof(proc_results));
    memset(proc_errors, 0, sizeof(proc_errors));
    memcpy(proc_cycles, cycles, sequence_length * sizeof(*cycles));
    memcpy(proc_results, results, sequence_length * sizeof(*results));
    memcpy(proc_errors, errors, sequence_length * sizeof(*errors));
    proc_sequence_length = sequence_length;
    proc_call_count = 0;
    reset_exception();
}

static void configure_proc_cpu_time_sequence(
    const uint64_t *user_times,
    const uint64_t *system_times,
    const int *results,
    const int *errors,
    size_t sequence_length
)
{
    const uint64_t zero_cycles[8] = {0};

    configure_proc_sequence(zero_cycles, results, errors, sequence_length);
    memcpy(proc_user_times, user_times, sequence_length * sizeof(*user_times));
    memcpy(proc_system_times, system_times, sequence_length * sizeof(*system_times));
}

static void configure_thread_id_sequence(
    const uint64_t *thread_id_values, const int *results, const int *errors, size_t sequence_length
)
{
    if (sequence_length > sizeof(thread_ids) / sizeof(thread_ids[0])) {
        fprintf(stderr, "thread ID sequence is too long\n");
        exit(EXIT_FAILURE);
    }

    memset(thread_ids, 0, sizeof(thread_ids));
    memset(thread_id_results, 0, sizeof(thread_id_results));
    memset(thread_id_errors, 0, sizeof(thread_id_errors));
    memcpy(thread_ids, thread_id_values, sequence_length * sizeof(*thread_id_values));
    memcpy(thread_id_results, results, sequence_length * sizeof(*results));
    memcpy(thread_id_errors, errors, sequence_length * sizeof(*errors));
    thread_id_sequence_length = sequence_length;
    thread_id_call_count = 0;
    reset_exception();
}

static void configure_thread_usage_sequence(
    const struct perfidious_darwin_thread_resource_usage *usages,
    const int *results,
    const int *errors,
    size_t sequence_length
)
{
    if (sequence_length > sizeof(thread_usages) / sizeof(thread_usages[0])) {
        fprintf(stderr, "thread usage sequence is too long\n");
        exit(EXIT_FAILURE);
    }

    memset(thread_usages, 0, sizeof(thread_usages));
    memset(thread_usage_results, 0, sizeof(thread_usage_results));
    memset(thread_usage_errors, 0, sizeof(thread_usage_errors));
    memcpy(thread_usages, usages, sequence_length * sizeof(*usages));
    memcpy(thread_usage_results, results, sequence_length * sizeof(*results));
    memcpy(thread_usage_errors, errors, sequence_length * sizeof(*errors));
    thread_usage_sequence_length = sequence_length;
    thread_usage_call_count = 0;
    reset_exception();
}

int proc_pid_rusage(int pid, int flavor, rusage_info_t *buffer)
{
    size_t call = proc_call_count++;
    struct rusage_info_v4 *usage;

    if (pid != getpid() || flavor != RUSAGE_INFO_V4 || buffer == NULL || call >= proc_sequence_length) {
        errno = EINVAL;
        return -1;
    }
    usage = (struct rusage_info_v4 *) buffer;
    if (proc_results[call] != 0) {
        errno = proc_errors[call];
        return proc_results[call];
    }

    memset(usage, 0, sizeof(*usage));
    usage->ri_user_time = proc_user_times[call];
    usage->ri_system_time = proc_system_times[call];
    usage->ri_cycles = proc_cycles[call];
    return 0;
}

zend_object *zend_throw_exception(zend_class_entry *exception_ce, const char *message, zend_long code)
{
    thrown_class = exception_ce;
    thrown_code = code;
    snprintf(thrown_message, sizeof(thrown_message), "%s", message);
    return NULL;
}

zend_object *zend_throw_exception_ex(zend_class_entry *exception_ce, zend_long code, const char *format, ...)
{
    va_list arguments;

    thrown_class = exception_ce;
    thrown_code = code;
    va_start(arguments, format);
    vsnprintf(thrown_message, sizeof(thrown_message), format, arguments);
    va_end(arguments);
    return NULL;
}

PERFIDIOUS_LOCAL zend_result perfidious_darwin_get_current_thread_id(uint64_t *thread_id)
{
    size_t call = thread_id_call_count++;

    if (call >= thread_id_sequence_length) {
        zend_throw_exception(perfidious_io_exception_ce, "unexpected pthread_threadid_np call", EINVAL);
        return FAILURE;
    }
    if (thread_id_results[call] != 0) {
        int error = thread_id_errors[call];

        zend_throw_exception_ex(
            perfidious_io_exception_ce, error, "pthread_threadid_np failed: [%d] %s", error, strerror(error)
        );
        return FAILURE;
    }

    *thread_id = thread_ids[call];
    return SUCCESS;
}

PERFIDIOUS_LOCAL zend_result
perfidious_darwin_read_current_thread_resource_usage(struct perfidious_darwin_thread_resource_usage *usage)
{
    size_t call = thread_usage_call_count++;

    if (call >= thread_usage_sequence_length) {
        zend_throw_exception(perfidious_io_exception_ce, "unexpected Darwin thread resource read", EINVAL);
        return FAILURE;
    }
    if (thread_usage_results[call] != 0) {
        int error = thread_usage_errors[call];

        zend_throw_exception_ex(
            perfidious_io_exception_ce, error, "Darwin thread resource read failed: [%d] %s", error, strerror(error)
        );
        return FAILURE;
    }

    *usage = thread_usages[call];
    return SUCCESS;
}

/* This sampler-only harness uses a 1:1 timebase; cpu-time-shim.phpt tests the real conversion. */
PERFIDIOUS_LOCAL bool perfidious_darwin_mach_time_to_ns(uint64_t value, uint64_t *result)
{
    *result = value;
    return true;
}

#ifndef PERFIDIOUS_DARWIN_SAMPLER_SOURCE
#define PERFIDIOUS_DARWIN_SAMPLER_SOURCE "../../src/darwin/sampler.c"
#endif
#ifndef PERFIDIOUS_TEST_PRELOAD_MARKER
#define PERFIDIOUS_TEST_PRELOAD_MARKER "perfidious-test-preload"
#endif
#include PERFIDIOUS_DARWIN_SAMPLER_SOURCE

#define INSTRUCTIONS_MASK PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_INSTRUCTIONS)
#define BASE_PROCESS_METRICS                                                                                           \
    (PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK)

#define CHECK(label, condition)                                                                                        \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL: %s\n", (label));                                                                    \
            failure_count++;                                                                                           \
        }                                                                                                              \
    } while (0)

static bool environment_variable_omits(const char *name, const char *marker)
{
    const char *value = getenv(name);

    return value == NULL || strstr(value, marker) == NULL;
}

static void test_preload_injection_is_cleared(void)
{
    CHECK(
        "synthetic LD_PRELOAD is not inherited",
        environment_variable_omits("LD_PRELOAD", PERFIDIOUS_TEST_PRELOAD_MARKER)
    );
    CHECK(
        "synthetic DYLD_INSERT_LIBRARIES is not inherited",
        environment_variable_omits("DYLD_INSERT_LIBRARIES", PERFIDIOUS_TEST_PRELOAD_MARKER)
    );
}

static void test_probe_is_request_scoped(void)
{
    const uint64_t cycles[] = {0};
    const int results[] = {-1};
    const int errors[] = {EIO};
    uint32_t supported = UINT32_MAX;

    configure_proc_sequence(cycles, results, errors, 1);
    CHECK(
        "base process support succeeds without probing",
        perfidious_platform_sampler_supported_metrics(
            PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK,
            PERFIDIOUS_SCOPE_CURRENT_PROCESS,
            &supported
        ) == SUCCESS
    );
    CHECK("base process metrics remain supported", supported == BASE_PROCESS_METRICS);
    CHECK("base process support did not call proc_pid_rusage", proc_call_count == 0);
    CHECK("base process support did not throw", thrown_class == NULL);

    supported = UINT32_MAX;
    CHECK(
        "instructions-only support succeeds without probing",
        perfidious_platform_sampler_supported_metrics(
            INSTRUCTIONS_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
        ) == SUCCESS
    );
    CHECK("instructions-only support did not call proc_pid_rusage", proc_call_count == 0);
    CHECK("instructions remain rejected", (INSTRUCTIONS_MASK & ~supported) == INSTRUCTIONS_MASK);

    supported = UINT32_MAX;
    CHECK(
        "thread support succeeds without probing",
        perfidious_platform_sampler_supported_metrics(
            PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK | INSTRUCTIONS_MASK,
            PERFIDIOUS_SCOPE_CURRENT_THREAD,
            &supported
        ) == SUCCESS
    );
    CHECK("thread CPU time is supported", supported == PERFIDIOUS_METRIC_CPU_TIME_MASK);
    CHECK("thread support did not call proc_pid_rusage", proc_call_count == 0);
}

static void test_thread_request_mask_matrix(void)
{
    const uint32_t all_metric_masks = PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_COUNT) - 1;
    const uint64_t cycles[] = {0};
    const int results[] = {-1};
    const int errors[] = {EIO};

    thread_id_call_count = 0;
    thread_usage_call_count = 0;
    for (uint32_t requested = 0; requested <= all_metric_masks; requested++) {
        uint32_t supported = UINT32_MAX;

        configure_proc_sequence(cycles, results, errors, 1);
        CHECK(
            "every current-thread request resolves without a fallible process probe",
            perfidious_platform_sampler_supported_metrics(requested, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported) ==
                SUCCESS
        );
        CHECK(
            "current-thread support contains exactly CPU time for every request mask",
            supported == PERFIDIOUS_METRIC_CPU_TIME_MASK
        );
        CHECK(
            "every requested non-CPU thread metric remains rejected",
            (requested & ~supported) == (requested & ~PERFIDIOUS_METRIC_CPU_TIME_MASK)
        );
        CHECK("current-thread support never probes process resources", proc_call_count == 0);
        CHECK("current-thread support never resolves a native thread ID", thread_id_call_count == 0);
        CHECK("current-thread support never reads native thread resources", thread_usage_call_count == 0);
        CHECK("current-thread support discovery does not throw", thrown_class == NULL);
    }
}

static void test_positive_probe_and_relative_readings(void)
{
    const uint64_t cycles[] = {100, 150, 205};
    const int results[] = {0, 0, 0};
    const int errors[] = {0, 0, 0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot origin;
    struct perfidious_sampler_snapshot current;
    uint32_t requested = PERFIDIOUS_METRIC_CPU_CYCLES_MASK;
    uint32_t supported = 0;

    configure_proc_sequence(cycles, results, errors, 3);
    CHECK(
        "positive cycle probe succeeds",
        perfidious_platform_sampler_supported_metrics(requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported) ==
            SUCCESS
    );
    CHECK(
        "positive cycle probe admits cycles", supported == (BASE_PROCESS_METRICS | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)
    );
    CHECK("positive support probe performs one native read", proc_call_count == 1);

    CHECK(
        "cycle sampler opens",
        perfidious_platform_sampler_open(
            PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &sampler
        ) == SUCCESS
    );
    CHECK("sampler open itself does not perform another native read", proc_call_count == 1);
    CHECK("origin read succeeds", perfidious_platform_sampler_read(sampler, &origin) == SUCCESS);
    CHECK("current read succeeds", perfidious_platform_sampler_read(sampler, &current) == SUCCESS);
    CHECK("origin uses the post-probe native value", origin.values[PERFIDIOUS_METRIC_CPU_CYCLES] == 150);
    CHECK("current uses the latest native value", current.values[PERFIDIOUS_METRIC_CPU_CYCLES] == 205);
    CHECK(
        "native snapshots yield the expected relative cycle reading",
        current.values[PERFIDIOUS_METRIC_CPU_CYCLES] - origin.values[PERFIDIOUS_METRIC_CPU_CYCLES] == 55
    );
    CHECK("probe plus origin and current perform exactly three native reads", proc_call_count == 3);
    CHECK("positive path does not throw", thrown_class == NULL);
    perfidious_platform_sampler_close(sampler);
}

static void test_thread_cpu_time_relative_readings(void)
{
    const uint64_t thread_id_values[] = {42, 42, 42};
    const int thread_id_results_values[] = {0, 0, 0};
    const int thread_id_errors_values[] = {0, 0, 0};
    const struct perfidious_darwin_thread_resource_usage usages[] = {
        {.user_time_ns = 5,  .system_time_ns = 3, .instructions = UINT64_MAX, .cycles = UINT64_MAX},
        {.user_time_ns = 14, .system_time_ns = 7, .instructions = UINT64_MAX, .cycles = UINT64_MAX},
    };
    const int usage_results[] = {0, 0};
    const int usage_errors[] = {0, 0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot origin;
    struct perfidious_sampler_snapshot current;

    configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 3);
    configure_thread_usage_sequence(usages, usage_results, usage_errors, 2);

    CHECK(
        "thread CPU-time sampler opens",
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) ==
            SUCCESS
    );
    CHECK("thread CPU-time origin read succeeds", perfidious_platform_sampler_read(sampler, &origin) == SUCCESS);
    CHECK("thread CPU-time current read succeeds", perfidious_platform_sampler_read(sampler, &current) == SUCCESS);
    CHECK("thread CPU-time origin combines user and system time", origin.values[PERFIDIOUS_METRIC_CPU_TIME] == 8);
    CHECK("thread CPU-time current combines user and system time", current.values[PERFIDIOUS_METRIC_CPU_TIME] == 21);
    CHECK(
        "thread CPU-time native snapshots yield the expected relative reading",
        current.values[PERFIDIOUS_METRIC_CPU_TIME] - origin.values[PERFIDIOUS_METRIC_CPU_TIME] == 13
    );
    for (enum perfidious_metric_id metric = PERFIDIOUS_METRIC_PAGE_FAULTS; metric < PERFIDIOUS_METRIC_COUNT; metric++) {
        CHECK("thread reads expose no unrequested metric values", origin.values[metric] == 0);
        CHECK("later thread reads expose no unrequested metric values", current.values[metric] == 0);
    }
    CHECK("opening and two reads check the native thread three times", thread_id_call_count == 3);
    CHECK("two thread CPU-time reads use two native snapshots", thread_usage_call_count == 2);
    CHECK("thread CPU-time path does not throw", thrown_class == NULL);
    perfidious_platform_sampler_close(sampler);
}

static void test_thread_identity_is_enforced(void)
{
    const uint64_t thread_id_values[] = {42, 43, 42};
    const int thread_id_results_values[] = {0, 0, 0};
    const int thread_id_errors_values[] = {0, 0, 0};
    const struct perfidious_darwin_thread_resource_usage usages[] = {
        {.user_time_ns = 11, .system_time_ns = 7},
    };
    const int usage_results[] = {0};
    const int usage_errors[] = {0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot current;

    configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 3);
    configure_thread_usage_sequence(usages, usage_results, usage_errors, 1);

    CHECK(
        "wrong-thread test sampler opens",
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) ==
            SUCCESS
    );
    CHECK("reading from another native thread fails", perfidious_platform_sampler_read(sampler, &current) == FAILURE);
    CHECK("wrong native thread throws WrongThreadException", thrown_class == perfidious_wrong_thread_exception_ce);
    CHECK("wrong native thread is rejected before reading resource usage", thread_usage_call_count == 0);
    reset_exception();
    CHECK(
        "the opening thread can read after a wrong-thread rejection",
        perfidious_platform_sampler_read(sampler, &current) == SUCCESS
    );
    CHECK("the recovered owner-thread read returns native CPU time", current.values[PERFIDIOUS_METRIC_CPU_TIME] == 18);
    CHECK("only the recovered owner-thread read consumes a native resource snapshot", thread_usage_call_count == 1);
    CHECK("wrong-thread rejection leaves the sampler reusable", thrown_class == NULL);
    perfidious_platform_sampler_close(sampler);
}

static void test_thread_identity_failures_are_io_errors(void)
{
    {
        const uint64_t thread_id_values[] = {0};
        const int thread_id_results_values[] = {EIO};
        const int thread_id_errors_values[] = {EIO};
        struct perfidious_platform_sampler *sampler = NULL;

        thread_usage_call_count = 0;
        configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 1);
        CHECK(
            "a native thread-ID failure prevents sampler open",
            perfidious_platform_sampler_open(
                PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler
            ) == FAILURE
        );
        CHECK("a failed native thread-ID open does not publish a sampler", sampler == NULL);
        CHECK("a native thread-ID open failure throws IOException", thrown_class == perfidious_io_exception_ce);
        CHECK("a native thread-ID open failure preserves its error code", thrown_code == EIO);
        CHECK(
            "a native thread-ID open failure identifies pthread_threadid_np",
            strstr(thrown_message, "pthread_threadid_np failed") != NULL
        );
        CHECK("a failed native thread-ID open never reads thread resources", thread_usage_call_count == 0);
    }

    {
        const uint64_t thread_id_values[] = {42, 0, 42};
        const int thread_id_results_values[] = {0, EAGAIN, 0};
        const int thread_id_errors_values[] = {0, EAGAIN, 0};
        const struct perfidious_darwin_thread_resource_usage usages[] = {
            {.user_time_ns = 21, .system_time_ns = 13},
        };
        const int usage_results[] = {0};
        const int usage_errors[] = {0};
        struct perfidious_platform_sampler *sampler = NULL;
        struct perfidious_sampler_snapshot snapshot;

        configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 3);
        configure_thread_usage_sequence(usages, usage_results, usage_errors, 1);
        CHECK(
            "thread-ID read-failure sampler opens",
            perfidious_platform_sampler_open(
                PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler
            ) == SUCCESS
        );
        CHECK(
            "a native thread-ID failure prevents sampler read",
            perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE
        );
        CHECK("a native thread-ID read failure throws IOException", thrown_class == perfidious_io_exception_ce);
        CHECK("a native thread-ID read failure preserves its error code", thrown_code == EAGAIN);
        CHECK("a native thread-ID read failure occurs before resource usage", thread_usage_call_count == 0);

        reset_exception();
        CHECK(
            "the owner thread can read after a native thread-ID failure",
            perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
        );
        CHECK("recovery after a native thread-ID failure returns CPU time", snapshot.values[0] == 34);
        CHECK("recovery after a native thread-ID failure reads resources once", thread_usage_call_count == 1);
        CHECK("native thread-ID failure leaves the sampler reusable", thrown_class == NULL);
        perfidious_platform_sampler_close(sampler);
    }
}

static void test_thread_resource_failures_are_io_errors(void)
{
    const uint64_t thread_id_values[] = {42, 42, 42};
    const int thread_id_results_values[] = {0, 0, 0};
    const int thread_id_errors_values[] = {0, 0, 0};
    const struct perfidious_darwin_thread_resource_usage usages[] = {
        {0},
        {.user_time_ns = 34, .system_time_ns = 21},
    };
    const int usage_results[] = {EIO, 0};
    const int usage_errors[] = {EIO, 0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot snapshot;

    configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 3);
    configure_thread_usage_sequence(usages, usage_results, usage_errors, 2);
    CHECK(
        "thread-resource failure sampler opens",
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) ==
            SUCCESS
    );
    CHECK(
        "a native thread-resource failure prevents sampler read",
        perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE
    );
    CHECK("a native thread-resource failure throws IOException", thrown_class == perfidious_io_exception_ce);
    CHECK("a native thread-resource failure preserves its error code", thrown_code == EIO);
    CHECK("a native thread-resource failure follows an owner-thread check", thread_id_call_count == 2);

    reset_exception();
    CHECK(
        "the owner thread can read after a native resource failure",
        perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
    );
    CHECK("recovery after a native resource failure returns CPU time", snapshot.values[0] == 55);
    CHECK("recovery consumes the second configured resource snapshot", thread_usage_call_count == 2);
    CHECK("native resource failure leaves the sampler reusable", thrown_class == NULL);
    perfidious_platform_sampler_close(sampler);
}

static void test_thread_cpu_time_sum_boundaries(void)
{
    const uint64_t thread_id_values[] = {42, 42, 42, 42};
    const int thread_id_results_values[] = {0, 0, 0, 0};
    const int thread_id_errors_values[] = {0, 0, 0, 0};
    const struct perfidious_darwin_thread_resource_usage usages[] = {
        {.user_time_ns = UINT64_MAX,     .system_time_ns = 0},
        {.user_time_ns = UINT64_MAX - 7, .system_time_ns = 7},
        {.user_time_ns = UINT64_MAX,     .system_time_ns = 1},
    };
    const int usage_results[] = {0, 0, 0};
    const int usage_errors[] = {0, 0, 0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot snapshot;

    configure_thread_id_sequence(thread_id_values, thread_id_results_values, thread_id_errors_values, 4);
    configure_thread_usage_sequence(usages, usage_results, usage_errors, 3);
    CHECK(
        "thread CPU-time boundary sampler opens",
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) ==
            SUCCESS
    );
    CHECK(
        "UINT64_MAX thread CPU time with zero system time succeeds",
        perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
    );
    CHECK("UINT64_MAX thread CPU time is preserved", snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == UINT64_MAX);
    CHECK(
        "split thread CPU time summing to UINT64_MAX succeeds",
        perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
    );
    CHECK("split UINT64_MAX thread CPU time is preserved", snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == UINT64_MAX);
    CHECK("thread CPU time one past UINT64_MAX fails", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    CHECK("thread CPU-time sum overflow throws OverflowException", thrown_class == perfidious_overflow_exception_ce);
    CHECK("thread CPU-time sum overflow uses code zero", thrown_code == 0);
    CHECK(
        "thread CPU-time sum overflow identifies total CPU time",
        strstr(thrown_message, "Darwin total CPU time overflow") != NULL
    );
    CHECK("thread CPU-time sum overflow does not expose wrapped output", snapshot.values[0] == 0);
    CHECK("all boundary reads validate the owner thread first", thread_id_call_count == 4);
    CHECK("all boundary reads consume one native resource snapshot", thread_usage_call_count == 3);
    perfidious_platform_sampler_close(sampler);
}

static void test_process_cpu_time_sum_boundaries(void)
{
    const uint64_t user_times[] = {UINT64_MAX, UINT64_MAX - 7, UINT64_MAX};
    const uint64_t system_times[] = {0, 7, 1};
    const int results[] = {0, 0, 0};
    const int errors[] = {0, 0, 0};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot snapshot;
    size_t thread_id_calls_before = thread_id_call_count;
    size_t thread_usage_calls_before = thread_usage_call_count;

    configure_proc_cpu_time_sequence(user_times, system_times, results, errors, 3);
    CHECK(
        "process CPU-time boundary sampler opens",
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &sampler) ==
            SUCCESS
    );
    CHECK(
        "UINT64_MAX process CPU time with zero system time succeeds",
        perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
    );
    CHECK("UINT64_MAX process CPU time is preserved", snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == UINT64_MAX);
    CHECK(
        "split process CPU time summing to UINT64_MAX succeeds",
        perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
    );
    CHECK("split UINT64_MAX process CPU time is preserved", snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == UINT64_MAX);
    CHECK(
        "process CPU time one past UINT64_MAX fails", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE
    );
    CHECK("process CPU-time sum overflow throws OverflowException", thrown_class == perfidious_overflow_exception_ce);
    CHECK("process CPU-time sum overflow uses code zero", thrown_code == 0);
    CHECK(
        "process CPU-time sum overflow identifies total CPU time",
        strstr(thrown_message, "Darwin total CPU time overflow") != NULL
    );
    CHECK("process CPU-time sum overflow does not expose wrapped output", snapshot.values[0] == 0);
    CHECK("process CPU reads consume exactly one process snapshot each", proc_call_count == 3);
    CHECK("process sampler open and reads never resolve a thread ID", thread_id_call_count == thread_id_calls_before);
    CHECK(
        "process sampler open and reads never consume thread resources",
        thread_usage_call_count == thread_usage_calls_before
    );
    perfidious_platform_sampler_close(sampler);
}

static void test_zero_probe_rejects_cycles(void)
{
    const uint64_t cycles[] = {0};
    const int results[] = {0};
    const int errors[] = {0};
    uint32_t requested = PERFIDIOUS_METRIC_CPU_CYCLES_MASK;
    uint32_t supported = 0;

    configure_proc_sequence(cycles, results, errors, 1);
    CHECK(
        "zero cycle probe completes",
        perfidious_platform_sampler_supported_metrics(requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported) ==
            SUCCESS
    );
    CHECK("zero cycle probe leaves only base metrics", supported == BASE_PROCESS_METRICS);
    CHECK("zero cycle request rejects cycles", (requested & ~supported) == PERFIDIOUS_METRIC_CPU_CYCLES_MASK);
    CHECK("zero cycle probe performs exactly one native read", proc_call_count == 1);
    CHECK("zero cycle probe does not throw an I/O exception", thrown_class == NULL);
}

static void test_known_unsupported_metric_takes_precedence_over_cycle_probe(void)
{
    const uint64_t cycles[] = {0};
    const int results[] = {-1};
    const int errors[] = {EIO};
    uint32_t requested = PERFIDIOUS_METRIC_CPU_CYCLES_MASK | INSTRUCTIONS_MASK;
    uint32_t supported = 0;

    configure_proc_sequence(cycles, results, errors, 1);
    CHECK(
        "known unsupported metric avoids a fallible cycle probe",
        perfidious_platform_sampler_supported_metrics(requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported) ==
            SUCCESS
    );
    CHECK("mixed request rejects only the known unsupported metric", (requested & ~supported) == INSTRUCTIONS_MASK);
    CHECK("known unsupported metric performs no native read", proc_call_count == 0);
    CHECK("known unsupported metric does not throw an I/O exception", thrown_class == NULL);
}

static void test_process_request_mask_matrix(void)
{
    const uint32_t all_metric_masks = PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_COUNT) - 1;

    for (uint32_t requested = 0; requested <= all_metric_masks; requested++) {
        const bool requests_instructions = (requested & INSTRUCTIONS_MASK) != 0;
        const bool requests_cycles = (requested & PERFIDIOUS_METRIC_CPU_CYCLES_MASK) != 0;
        uint32_t supported = UINT32_MAX;

        if (requests_instructions) {
            const uint64_t cycles[] = {0};
            const int results[] = {1};
            const int errors[] = {EPERM};

            configure_proc_sequence(cycles, results, errors, 1);
            CHECK(
                "every statically unsupported process request bypasses the cycle probe",
                perfidious_platform_sampler_supported_metrics(
                    requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
                ) == SUCCESS
            );
            CHECK(
                "static rejection preserves every base process metric",
                (supported & BASE_PROCESS_METRICS) == BASE_PROCESS_METRICS
            );
            CHECK(
                "static rejection invents no process metric",
                (supported & ~(BASE_PROCESS_METRICS | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)) == 0
            );
            CHECK(
                "static rejection rejects instructions and no requested supported metric",
                (requested & ~supported) == INSTRUCTIONS_MASK
            );
            CHECK("static rejection performs no native probe", proc_call_count == 0);
            CHECK("static rejection cannot be replaced by a native exception", thrown_class == NULL);
            continue;
        }

        if (!requests_cycles) {
            const uint64_t cycles[] = {0};
            const int results[] = {1};
            const int errors[] = {EPERM};

            configure_proc_sequence(cycles, results, errors, 1);
            CHECK(
                "every base-only process request succeeds without probing",
                perfidious_platform_sampler_supported_metrics(
                    requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
                ) == SUCCESS
            );
            CHECK("base-only process requests expose exactly the base mask", supported == BASE_PROCESS_METRICS);
            CHECK("base-only process requests reject no requested metric", (requested & ~supported) == 0);
            CHECK("base-only process requests perform no native probe", proc_call_count == 0);
            CHECK("base-only process requests do not throw", thrown_class == NULL);
            continue;
        }

        {
            const uint64_t cycles[] = {0};
            const int results[] = {0};
            const int errors[] = {0};

            configure_proc_sequence(cycles, results, errors, 1);
            CHECK(
                "every cycle request completes a zero availability probe",
                perfidious_platform_sampler_supported_metrics(
                    requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
                ) == SUCCESS
            );
            CHECK("a zero probe exposes exactly the base mask", supported == BASE_PROCESS_METRICS);
            CHECK(
                "a zero probe rejects cycles and no requested base metric",
                (requested & ~supported) == PERFIDIOUS_METRIC_CPU_CYCLES_MASK
            );
            CHECK("each zero cycle request performs exactly one native probe", proc_call_count == 1);
            CHECK("a zero probe does not become an I/O exception", thrown_class == NULL);
        }

        {
            const uint64_t cycles[] = {1};
            const int results[] = {0};
            const int errors[] = {0};

            supported = UINT32_MAX;
            configure_proc_sequence(cycles, results, errors, 1);
            CHECK(
                "the smallest positive cycle value admits every cycle request",
                perfidious_platform_sampler_supported_metrics(
                    requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
                ) == SUCCESS
            );
            CHECK(
                "a positive probe exposes exactly the nominal process mask",
                supported == (BASE_PROCESS_METRICS | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)
            );
            CHECK("a positive probe rejects no requested metric", (requested & ~supported) == 0);
            CHECK("each positive cycle request performs exactly one native probe", proc_call_count == 1);
            CHECK("a positive probe does not throw", thrown_class == NULL);
        }

        {
            const uint64_t cycles[] = {0};
            const int results[] = {1};
            const int errors[] = {EPERM};

            supported = UINT32_MAX;
            configure_proc_sequence(cycles, results, errors, 1);
            CHECK(
                "every cycle request propagates any nonzero native result",
                perfidious_platform_sampler_supported_metrics(
                    requested, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
                ) == FAILURE
            );
            CHECK("a positive native error result throws IOException", thrown_class == perfidious_io_exception_ce);
            CHECK("a positive native error result preserves errno", thrown_code == EPERM);
            CHECK(
                "a native error identifies proc_pid_rusage", strstr(thrown_message, "proc_pid_rusage failed") != NULL
            );
            CHECK("each failing cycle request performs exactly one native probe", proc_call_count == 1);
        }
    }
}

static void test_native_probe_error_is_io_failure(void)
{
    const uint64_t cycles[] = {0};
    const int results[] = {-1};
    const int errors[] = {EIO};
    uint32_t supported = 0;

    configure_proc_sequence(cycles, results, errors, 1);
    CHECK(
        "native cycle probe failure propagates",
        perfidious_platform_sampler_supported_metrics(
            PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
        ) == FAILURE
    );
    CHECK("native failure throws IOException", thrown_class == perfidious_io_exception_ce);
    CHECK("native failure preserves errno as exception code", thrown_code == EIO);
    CHECK("native failure identifies proc_pid_rusage", strstr(thrown_message, "proc_pid_rusage failed") != NULL);
    CHECK("native failure performs exactly one native read", proc_call_count == 1);
}

static void test_native_sample_error_is_io_failure(void)
{
    const uint64_t cycles[] = {100, 0};
    const int results[] = {0, -1};
    const int errors[] = {0, EIO};
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot snapshot;
    uint32_t supported = 0;

    configure_proc_sequence(cycles, results, errors, 2);
    CHECK(
        "cycle support probe succeeds before a later native error",
        perfidious_platform_sampler_supported_metrics(
            PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported
        ) == SUCCESS
    );
    CHECK(
        "cycle sampler opens before a later native error",
        perfidious_platform_sampler_open(
            PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &sampler
        ) == SUCCESS
    );
    CHECK("native sample failure propagates", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    CHECK("native sample failure throws IOException", thrown_class == perfidious_io_exception_ce);
    CHECK("native sample failure preserves errno as exception code", thrown_code == EIO);
    CHECK("native sample failure identifies proc_pid_rusage", strstr(thrown_message, "proc_pid_rusage failed") != NULL);
    CHECK("probe and failing sample perform exactly two native reads", proc_call_count == 2);
    perfidious_platform_sampler_close(sampler);
}

int main(void)
{
    test_preload_injection_is_cleared();
    test_probe_is_request_scoped();
    test_thread_request_mask_matrix();
    test_positive_probe_and_relative_readings();
    test_thread_cpu_time_relative_readings();
    test_thread_identity_is_enforced();
    test_thread_identity_failures_are_io_errors();
    test_thread_resource_failures_are_io_errors();
    test_thread_cpu_time_sum_boundaries();
    test_process_cpu_time_sum_boundaries();
    test_zero_probe_rejects_cycles();
    test_known_unsupported_metric_takes_precedence_over_cycle_probe();
    test_process_request_mask_matrix();
    test_native_probe_error_is_io_failure();
    test_native_sample_error_is_io_failure();

    if (failure_count != 0) {
        fprintf(stderr, "%d Darwin sampler probe assertion(s) failed\n", failure_count);
        return EXIT_FAILURE;
    }

    puts("Darwin sampler probe harness passed");
    return EXIT_SUCCESS;
}
