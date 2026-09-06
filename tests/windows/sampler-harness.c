/*
 * Standalone component tests for the real Windows sampler and profiling wrappers.
 * Native calls are controlled here; Windows SDK/ABI and Zend runtime behavior are
 * covered separately by the native Windows PHPTs.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"
#include "../../php_perfidious.h"
#include "../../src/sampler.h"
#include "windows.h"
#include "psapi.h"

#undef snprintf
#undef vsnprintf

static const char *test_case;
#define CHECK(label, condition)                                                                                        \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "%s: %s (line %d)\n", test_case, label, __LINE__);                                         \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

static zend_class_entry io_class, overflow_class, wrong_thread_class, busy_class;
zend_class_entry *perfidious_io_exception_ce = &io_class;
zend_class_entry *perfidious_overflow_exception_ce = &overflow_class;
zend_class_entry *perfidious_wrong_thread_exception_ce = &wrong_thread_class;
zend_class_entry *perfidious_resource_busy_exception_ce = &busy_class;

enum operation
{
    NONE,
    DUPLICATE,
    ENABLE_PROFILE,
    WAIT,
    PROCESS_TIMES,
    THREAD_TIMES,
    MEMORY,
    CYCLES,
    PROFILE_READ
};
enum handle_kind
{
    THREAD_HANDLE,
    PROFILE_HANDLE
};
static struct native_handle
{
    enum handle_kind kind;
    DWORD thread_id;
    bool live;
} handles[16];
static size_t handle_count, live_allocations, allocations_at_exception, handles_at_exception;
static unsigned int close_calls, disable_calls, native_reads;
static char process_token, thread_token;
static DWORD current_thread_id, last_error, failure_code, wait_result;
static enum operation failed_operation;
static uint64_t kernel_ticks, user_ticks, cycle_count;
static DWORD page_faults, context_switches;
static zend_class_entry *thrown_class;
static zend_long thrown_code;
static char thrown_message[256];

static size_t live_handles(void)
{
    size_t count = 0;
    for (size_t i = 0; i < handle_count; i++) {
        count += handles[i].live;
    }
    return count;
}

static void clear_exception(void)
{
    thrown_class = NULL;
    thrown_code = 0;
    thrown_message[0] = '\0';
}

static void reset_fixture(const char *name)
{
    test_case = name;
    CHECK("previous case released sampler allocations", live_allocations == 0);
    CHECK("previous case released native handles", live_handles() == 0);
    memset(handles, 0, sizeof(handles));
    handle_count = close_calls = disable_calls = native_reads = 0;
    current_thread_id = 41;
    last_error = ERROR_INVALID_PARAMETER;
    failure_code = ERROR_ACCESS_DENIED;
    failed_operation = NONE;
    wait_result = WAIT_TIMEOUT;
    kernel_ticks = 7;
    user_ticks = 11;
    cycle_count = UINT64_C(1099511627793);
    page_faults = 9;
    context_switches = 13;
    clear_exception();
}

static void *allocate_sampler(size_t count, size_t size)
{
    void *result = calloc(count, size);
    CHECK("test allocator succeeded", result != NULL);
    live_allocations++;
    return result;
}

static void free_sampler(void *pointer)
{
    CHECK("free has an owned sampler", pointer != NULL && live_allocations > 0);
    live_allocations--;
    free(pointer);
    last_error = ERROR_INVALID_HANDLE; /* Cleanup may overwrite GetLastError(). */
}

#undef ecalloc
#define ecalloc(count, size) allocate_sampler((count), (size))
#undef efree
#define efree(pointer) free_sampler(pointer)

static void record_exception(zend_class_entry *exception, zend_long code)
{
    CHECK("no exception was silently replaced", thrown_class == NULL);
    thrown_class = exception;
    thrown_code = code;
    allocations_at_exception = live_allocations;
    handles_at_exception = live_handles();
}

zend_object *zend_throw_exception(zend_class_entry *exception, const char *message, zend_long code)
{
    record_exception(exception, code);
    snprintf(thrown_message, sizeof(thrown_message), "%s", message);
    return NULL;
}

zend_object *zend_throw_exception_ex(zend_class_entry *exception, zend_long code, const char *format, ...)
{
    va_list arguments;
    record_exception(exception, code);
    va_start(arguments, format);
    vsnprintf(thrown_message, sizeof(thrown_message), format, arguments);
    va_end(arguments);
    return NULL;
}

static HANDLE new_handle(enum handle_kind kind)
{
    CHECK("fixture has handle capacity", handle_count < sizeof(handles) / sizeof(handles[0]));
    struct native_handle *result = &handles[handle_count++];
    result->kind = kind;
    result->thread_id = current_thread_id;
    result->live = true;
    return result;
}

static struct native_handle *owned_handle(HANDLE value, enum handle_kind kind)
{
    for (size_t i = 0; i < handle_count; i++) {
        if (value == &handles[i]) {
            CHECK("native handle is still live", handles[i].live);
            CHECK("native handle has the correct kind", handles[i].kind == kind);
            return &handles[i];
        }
    }
    CHECK("native handle belongs to the caller", false);
    return NULL;
}

static BOOL operation_succeeds(enum operation operation)
{
    if (failed_operation == operation) {
        last_error = failure_code;
        return FALSE;
    }
    return TRUE;
}

HANDLE GetCurrentProcess(void)
{
    return &process_token;
}
HANDLE GetCurrentThread(void)
{
    return &thread_token;
}
DWORD GetCurrentThreadId(void)
{
    return current_thread_id;
}
DWORD GetLastError(void)
{
    return last_error;
}

BOOL DuplicateHandle(
    HANDLE source_process,
    HANDLE source,
    HANDLE target_process,
    HANDLE *target,
    DWORD access,
    BOOL inherit,
    DWORD options
)
{
    CHECK(
        "duplicate current thread in current process",
        source_process == &process_token && target_process == &process_token && source == &thread_token
    );
    CHECK(
        "duplicate preserves access without inheritance",
        access == 0 && inherit == FALSE && options == DUPLICATE_SAME_ACCESS
    );
    if (!operation_succeeds(DUPLICATE)) {
        return FALSE;
    }
    *target = new_handle(THREAD_HANDLE);
    return TRUE;
}

BOOL CloseHandle(HANDLE handle)
{
    owned_handle(handle, THREAD_HANDLE)->live = false;
    close_calls++;
    last_error = ERROR_INVALID_HANDLE;
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    owned_handle(handle, THREAD_HANDLE);
    CHECK("thread identity probe never blocks", milliseconds == 0);
    if (!operation_succeeds(WAIT)) {
        return WAIT_FAILED;
    }
    return wait_result;
}

static BOOL
read_times(enum operation operation, FILETIME *creation, FILETIME *exit_time, FILETIME *kernel, FILETIME *user)
{
    native_reads++;
    if (!operation_succeeds(operation)) {
        return FALSE;
    }
    memset(creation, 0, sizeof(*creation));
    memset(exit_time, 0, sizeof(*exit_time));
    kernel->dwLowDateTime = (DWORD) kernel_ticks;
    kernel->dwHighDateTime = (DWORD) (kernel_ticks >> 32);
    user->dwLowDateTime = (DWORD) user_ticks;
    user->dwHighDateTime = (DWORD) (user_ticks >> 32);
    return TRUE;
}

BOOL GetProcessTimes(HANDLE handle, FILETIME *creation, FILETIME *exit_time, FILETIME *kernel, FILETIME *user)
{
    CHECK("CPU time uses current process", handle == &process_token);
    return read_times(PROCESS_TIMES, creation, exit_time, kernel, user);
}

BOOL GetThreadTimes(HANDLE handle, FILETIME *creation, FILETIME *exit_time, FILETIME *kernel, FILETIME *user)
{
    CHECK("CPU time uses the owned thread", owned_handle(handle, THREAD_HANDLE)->thread_id == current_thread_id);
    return read_times(THREAD_TIMES, creation, exit_time, kernel, user);
}

BOOL GetProcessMemoryInfo(HANDLE process, PROCESS_MEMORY_COUNTERS *memory, DWORD size)
{
    native_reads++;
    CHECK("page faults use current process", process == &process_token);
    CHECK("memory query initializes its structure", size == sizeof(*memory) && memory->cb == size);
    if (!operation_succeeds(MEMORY)) {
        return FALSE;
    }
    memory->PageFaultCount = page_faults;
    return TRUE;
}

BOOL QueryProcessCycleTime(HANDLE handle, ULONG64 *cycles)
{
    native_reads++;
    CHECK("cycles use current process", handle == &process_token);
    if (!operation_succeeds(CYCLES)) {
        return FALSE;
    }
    *cycles = cycle_count;
    return TRUE;
}

DWORD EnableThreadProfiling(HANDLE thread, DWORD flags, DWORD64 mask, HANDLE *profile)
{
    CHECK(
        "profiling requests dispatch data on the current thread",
        thread == &thread_token && flags == THREAD_PROFILING_FLAG_DISPATCH && mask == 0
    );
    if (failed_operation == ENABLE_PROFILE) {
        return failure_code; /* Direct return code; intentionally leave GetLastError stale. */
    }
    for (size_t i = 0; i < handle_count; i++) {
        if (handles[i].live && handles[i].kind == PROFILE_HANDLE && handles[i].thread_id == current_thread_id) {
            return ERROR_WMI_ALREADY_ENABLED;
        }
    }
    *profile = new_handle(PROFILE_HANDLE);
    return ERROR_SUCCESS;
}

DWORD ReadThreadProfilingData(HANDLE profile, DWORD flags, PERFORMANCE_DATA *data)
{
    native_reads++;
    CHECK("profiling uses the owned thread", owned_handle(profile, PROFILE_HANDLE)->thread_id == current_thread_id);
    CHECK(
        "profiling initializes Size and Version",
        data->Size == sizeof(*data) && data->Version == PERFORMANCE_DATA_VERSION
    );
    CHECK("profiling reads dispatch counters", flags == READ_THREAD_PROFILING_FLAG_DISPATCHING);
    if (failed_operation == PROFILE_READ) {
        return failure_code;
    }
    data->ContextSwitchCount = context_switches;
    data->CycleTime = cycle_count;
    return ERROR_SUCCESS;
}

DWORD DisableThreadProfiling(HANDLE profile)
{
    struct native_handle *handle = owned_handle(profile, PROFILE_HANDLE);
    CHECK("profiling is released by its owning thread", handle->thread_id == current_thread_id);
    handle->live = false;
    disable_calls++;
    return ERROR_SUCCESS;
}

#include "../../src/windows/thread_profile.c"
#ifndef PERFIDIOUS_WINDOWS_SAMPLER_SOURCE
#define PERFIDIOUS_WINDOWS_SAMPLER_SOURCE "../../src/windows/sampler.c"
#endif
#include PERFIDIOUS_WINDOWS_SAMPLER_SOURCE

#define PROCESS_METRICS                                                                                                \
    (PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)
#define THREAD_METRICS                                                                                                 \
    (PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK)

static struct perfidious_platform_sampler *open_sampler(uint32_t metrics, enum perfidious_scope_id scope)
{
    struct perfidious_platform_sampler *sampler = NULL;
    CHECK("sampler opens", perfidious_platform_sampler_open(metrics, scope, &sampler) == SUCCESS);
    CHECK("successful acquisition has no exception", thrown_class == NULL);
    return sampler;
}

static void check_error(zend_class_entry *expected, zend_long code, const char *operation)
{
    CHECK("exception class matches failure", thrown_class == expected);
    CHECK("native error survives cleanup", thrown_code == code);
    CHECK("diagnostic identifies failed operation", strstr(thrown_message, operation) != NULL);
    clear_exception();
}

static void check_read(struct perfidious_platform_sampler *sampler, enum perfidious_metric_id metric, uint64_t value)
{
    struct perfidious_sampler_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK("sampler read succeeds", perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    CHECK("successful read has no exception", thrown_class == NULL);
    CHECK("snapshot has exact expected count", snapshot.values[metric] == value);
    CHECK("unrequested instructions stay zero", snapshot.values[PERFIDIOUS_METRIC_INSTRUCTIONS] == 0);
}

static void test_acquisition_failures(void)
{
    const struct
    {
        enum operation operation;
        DWORD error;
        const char *name;
    } cases[] = {
        {DUPLICATE,      ERROR_ACCESS_DENIED,       "DuplicateHandle"      },
        {ENABLE_PROFILE, ERROR_ACCESS_DENIED,       "EnableThreadProfiling"},
        {ENABLE_PROFILE, ERROR_WMI_ALREADY_ENABLED, "EnableThreadProfiling"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_fixture(cases[i].name);
        struct perfidious_platform_sampler *existing = open_sampler(PROCESS_METRICS, PERFIDIOUS_SCOPE_CURRENT_PROCESS);
        struct perfidious_platform_sampler *output = existing;
        failed_operation = cases[i].operation;
        failure_code = cases[i].error;
        CHECK(
            "acquisition reports failure",
            perfidious_platform_sampler_open(THREAD_METRICS, PERFIDIOUS_SCOPE_CURRENT_THREAD, &output) == FAILURE
        );
        CHECK("failed acquisition leaves output ownership unchanged", output == existing);
        CHECK("new sampler freed before exception allocation", allocations_at_exception == 1);
        CHECK("new native handles freed before exception allocation", handles_at_exception == 0);
        check_error(
            cases[i].error == ERROR_WMI_ALREADY_ENABLED ? &busy_class : &io_class, cases[i].error, cases[i].name
        );
        failed_operation = NONE;
        struct perfidious_platform_sampler *retry = open_sampler(THREAD_METRICS, PERFIDIOUS_SCOPE_CURRENT_THREAD);
        check_read(retry, PERFIDIOUS_METRIC_CPU_TIME, 1800);
        perfidious_platform_sampler_close(retry);
        perfidious_platform_sampler_close(existing);
        CHECK("recovered sampler disables profiling once", disable_calls == 1);
        CHECK("every duplicated thread handle is closed", close_calls == (cases[i].operation == DUPLICATE ? 1u : 2u));
    }
}

static void test_profile_ownership(void)
{
    reset_fixture("profile ownership");
    failed_operation = ENABLE_PROFILE;
    struct perfidious_platform_sampler *cpu =
        open_sampler(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD);
    CHECK("thread CPU time needs no profiling handle", live_handles() == 1);
    check_read(cpu, PERFIDIOUS_METRIC_CPU_TIME, 1800);
    perfidious_platform_sampler_close(cpu);
    CHECK("CPU-only cleanup does not disable profiling", disable_calls == 0);

    failed_operation = NONE;
    struct perfidious_platform_sampler *owner = open_sampler(THREAD_METRICS, PERFIDIOUS_SCOPE_CURRENT_THREAD);
    struct perfidious_platform_sampler *rejected = NULL;
    CHECK(
        "second profiler on same thread is rejected",
        perfidious_platform_sampler_open(THREAD_METRICS, PERFIDIOUS_SCOPE_CURRENT_THREAD, &rejected) == FAILURE
    );
    CHECK("busy failure publishes no sampler", rejected == NULL);
    CHECK("busy failure retains only original ownership", allocations_at_exception == 1 && handles_at_exception == 2);
    check_error(&busy_class, ERROR_WMI_ALREADY_ENABLED, "EnableThreadProfiling");
    check_read(owner, PERFIDIOUS_METRIC_CONTEXT_SWITCHES, 13);
    perfidious_platform_sampler_close(owner);
    owner = open_sampler(PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD);
    CHECK("cycle-only thread sampler owns a profiling handle", live_handles() == 2);
    check_read(owner, PERFIDIOUS_METRIC_CPU_CYCLES, UINT64_C(1099511627793));
    perfidious_platform_sampler_close(owner);
    CHECK("both profile owners released exactly once", disable_calls == 2);
    CHECK("CPU, busy, and two profile duplicates released", close_calls == 4);
}

static void test_read_failures(void)
{
    const struct
    {
        enum operation operation;
        enum perfidious_scope_id scope;
        const char *name;
    } cases[] = {
        {PROCESS_TIMES, PERFIDIOUS_SCOPE_CURRENT_PROCESS, "GetProcessTimes"        },
        {MEMORY,        PERFIDIOUS_SCOPE_CURRENT_PROCESS, "GetProcessMemoryInfo"   },
        {CYCLES,        PERFIDIOUS_SCOPE_CURRENT_PROCESS, "QueryProcessCycleTime"  },
        {WAIT,          PERFIDIOUS_SCOPE_CURRENT_THREAD,  "WaitForSingleObject"    },
        {THREAD_TIMES,  PERFIDIOUS_SCOPE_CURRENT_THREAD,  "GetThreadTimes"         },
        {PROFILE_READ,  PERFIDIOUS_SCOPE_CURRENT_THREAD,  "ReadThreadProfilingData"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_fixture(cases[i].name);
        bool thread = cases[i].scope == PERFIDIOUS_SCOPE_CURRENT_THREAD;
        struct perfidious_platform_sampler *sampler =
            open_sampler(thread ? THREAD_METRICS : PROCESS_METRICS, cases[i].scope);
        struct perfidious_sampler_snapshot snapshot;
        failed_operation = cases[i].operation;
        CHECK("native read failure is reported", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
        check_error(&io_class, ERROR_ACCESS_DENIED, cases[i].name);
        CHECK("failed read preserves sampler ownership", live_allocations == 1 && live_handles() == (thread ? 2u : 0u));
        failed_operation = NONE;
        memset(&snapshot, 0xa5, sizeof(snapshot));
        CHECK(
            "sampler can retry after native read failure",
            perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS
        );
        CHECK("retry returns CPU nanoseconds", snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == 1800);
        CHECK(
            "retry returns page faults only for process",
            snapshot.values[PERFIDIOUS_METRIC_PAGE_FAULTS] == (thread ? 0u : 9u)
        );
        CHECK(
            "retry returns switches only for thread",
            snapshot.values[PERFIDIOUS_METRIC_CONTEXT_SWITCHES] == (thread ? 13u : 0u)
        );
        CHECK(
            "retry preserves 64-bit cycle count",
            snapshot.values[PERFIDIOUS_METRIC_CPU_CYCLES] == UINT64_C(1099511627793)
        );
        CHECK("retry clears unrequested instructions", snapshot.values[PERFIDIOUS_METRIC_INSTRUCTIONS] == 0);
        CHECK("retry raises no new exception", thrown_class == NULL);
        perfidious_platform_sampler_close(sampler);
    }
}

static void test_thread_identity(void)
{
    reset_fixture("thread identity");
    struct perfidious_platform_sampler *sampler = open_sampler(THREAD_METRICS, PERFIDIOUS_SCOPE_CURRENT_THREAD);
    struct perfidious_sampler_snapshot snapshot;
    current_thread_id = 42;
    CHECK("foreign thread cannot read sampler", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    check_error(&wrong_thread_class, 0, "thread");
    CHECK("foreign thread reads no counters", native_reads == 0);
    current_thread_id = 41;
    check_read(sampler, PERFIDIOUS_METRIC_CPU_TIME, 1800);
    perfidious_platform_sampler_close(sampler);

    reset_fixture("terminated thread ID reuse");
    sampler = open_sampler(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD);
    wait_result = WAIT_OBJECT_0;
    CHECK(
        "reused ID of terminated owner cannot read sampler",
        perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE
    );
    check_error(&wrong_thread_class, 0, "thread");
    CHECK("signaled thread reads no counters", native_reads == 0);
    perfidious_platform_sampler_close(sampler);
}

static void test_cpu_time_boundaries(enum perfidious_scope_id scope)
{
    reset_fixture(scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS ? "process CPU width" : "thread CPU width");
    struct perfidious_platform_sampler *sampler = open_sampler(PERFIDIOUS_METRIC_CPU_TIME_MASK, scope);
    struct perfidious_sampler_snapshot snapshot;
    kernel_ticks = UINT64_C(4294967298);
    user_ticks = UINT64_C(12884901892);
    check_read(sampler, PERFIDIOUS_METRIC_CPU_TIME, UINT64_C(1717986919000));
    kernel_ticks = UINT64_C(184467440737095510);
    user_ticks = 6;
    check_read(sampler, PERFIDIOUS_METRIC_CPU_TIME, UINT64_C(18446744073709551600));
    user_ticks = 7;
    CHECK("nanosecond conversion overflow fails", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    check_error(&overflow_class, 0, "nanosecond");
    kernel_ticks = UINT64_MAX;
    user_ticks = 1;
    CHECK("CPU-time addition overflow fails", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    check_error(&overflow_class, 0, "total CPU time");
    kernel_ticks = 19;
    user_ticks = 31;
    check_read(sampler, PERFIDIOUS_METRIC_CPU_TIME, 5000);
    perfidious_platform_sampler_close(sampler);
}

static void test_counter_wraps(enum perfidious_scope_id scope, enum perfidious_metric_id metric)
{
    const DWORD raw[] = {2147483647u, 2147483648u, 4294967294u, 4294967294u, 3u, 4294967295u, 0u, 5u};
    const uint64_t expected[] = {
        UINT64_C(2147483647),
        UINT64_C(2147483648),
        UINT64_C(4294967294),
        UINT64_C(4294967294),
        UINT64_C(4294967299),
        UINT64_C(8589934591),
        UINT64_C(8589934592),
        UINT64_C(8589934597)
    };
    const uint64_t later_expected[] = {3, UINT64_C(4294967295), UINT64_C(4294967296), UINT64_C(4294967301)};
    reset_fixture(scope == PERFIDIOUS_SCOPE_CURRENT_PROCESS ? "page-fault wraps" : "context-switch wraps");
    struct perfidious_platform_sampler *first = open_sampler(PERFIDIOUS_METRIC_MASK(metric), scope);
    struct perfidious_platform_sampler *later = NULL;
    for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); i++) {
        page_faults = context_switches = raw[i];
        current_thread_id = 41;
        check_read(first, metric, expected[i]);
        if (i >= 4) {
            current_thread_id = 42;
            if (later == NULL) {
                later = open_sampler(PERFIDIOUS_METRIC_MASK(metric), scope);
            }
            check_read(later, metric, later_expected[i - 4]);
        }
    }
    current_thread_id = 41;
    perfidious_platform_sampler_close(first);
    current_thread_id = 42;
    check_read(later, metric, UINT64_C(4294967301));
    perfidious_platform_sampler_close(later);
    later = open_sampler(PERFIDIOUS_METRIC_MASK(metric), scope);
    check_read(later, metric, 5);
    perfidious_platform_sampler_close(later);
}

static void test_wrap_during_failed_read(void)
{
    reset_fixture("wrap during failed cycle query");
    struct perfidious_platform_sampler *sampler = open_sampler(
        PERFIDIOUS_METRIC_PAGE_FAULTS_MASK | PERFIDIOUS_METRIC_CPU_CYCLES_MASK, PERFIDIOUS_SCOPE_CURRENT_PROCESS
    );
    struct perfidious_sampler_snapshot snapshot;
    page_faults = 4294967293u;
    check_read(sampler, PERFIDIOUS_METRIC_PAGE_FAULTS, UINT64_C(4294967293));
    page_faults = 2;
    failed_operation = CYCLES;
    CHECK("later native failure rejects the snapshot", perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    check_error(&io_class, ERROR_ACCESS_DENIED, "QueryProcessCycleTime");
    failed_operation = NONE;
    page_faults = 4;
    check_read(sampler, PERFIDIOUS_METRIC_PAGE_FAULTS, UINT64_C(4294967300));
    check_read(sampler, PERFIDIOUS_METRIC_PAGE_FAULTS, UINT64_C(4294967300));
    perfidious_platform_sampler_close(sampler);
}

int main(void)
{
    test_acquisition_failures();
    test_profile_ownership();
    test_read_failures();
    test_thread_identity();
    test_cpu_time_boundaries(PERFIDIOUS_SCOPE_CURRENT_PROCESS);
    test_cpu_time_boundaries(PERFIDIOUS_SCOPE_CURRENT_THREAD);
    test_counter_wraps(PERFIDIOUS_SCOPE_CURRENT_PROCESS, PERFIDIOUS_METRIC_PAGE_FAULTS);
    test_counter_wraps(PERFIDIOUS_SCOPE_CURRENT_THREAD, PERFIDIOUS_METRIC_CONTEXT_SWITCHES);
    test_wrap_during_failed_read();
    reset_fixture("final cleanup");
    puts("Windows sampler harness passed");
    return EXIT_SUCCESS;
}
