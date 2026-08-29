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

zend_class_entry *perfidious_io_exception_ce = &io_exception_class;
zend_class_entry *perfidious_overflow_exception_ce = &overflow_exception_class;

static zend_class_entry *thrown_class;
static zend_long thrown_code;
static char thrown_message[256];

static uint64_t proc_cycles[8];
static int proc_results[8];
static int proc_errors[8];
static size_t proc_sequence_length;
static size_t proc_call_count;

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
    memset(proc_results, 0, sizeof(proc_results));
    memset(proc_errors, 0, sizeof(proc_errors));
    memcpy(proc_cycles, cycles, sequence_length * sizeof(*cycles));
    memcpy(proc_results, results, sequence_length * sizeof(*results));
    memcpy(proc_errors, errors, sequence_length * sizeof(*errors));
    proc_sequence_length = sequence_length;
    proc_call_count = 0;
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
            PERFIDIOUS_METRIC_CPU_CYCLES_MASK | INSTRUCTIONS_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported
        ) == SUCCESS
    );
    CHECK("all thread metrics remain unsupported", supported == 0);
    CHECK("thread support did not call proc_pid_rusage", proc_call_count == 0);
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
    test_positive_probe_and_relative_readings();
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
