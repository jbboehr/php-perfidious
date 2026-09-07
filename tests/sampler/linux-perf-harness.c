/* Exercise the Linux sampler against controlled perf syscalls. */
#include <errno.h>
#include <linux/perf_event.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "main/php.h"
#include "Zend/zend_exceptions.h"
#include "../../src/sampler.h"

#undef snprintf
#undef vsnprintf
#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "Linux perf fixture failed at line %d: %s\n", __LINE__, #condition);                       \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

static zend_class_entry io_class, overflow_class, thread_class;
zend_class_entry *perfidious_io_exception_ce = &io_class;
zend_class_entry *perfidious_overflow_exception_ce = &overflow_class;
zend_class_entry *perfidious_wrong_thread_exception_ce = &thread_class;
static zend_class_entry *thrown;
static zend_long thrown_code;
static size_t allocations, live_fds, fds_at_exception, perf_open_calls, read_calls;
static int owner_tid = 123, next_fd = 10, fail_open_config = -1, fail_open_type = -1, fail_errno = EACCES;
static int fail_read_fd = -1, interrupted_read_fd = -1, interrupted_read_count, short_read_fd = -1;
static struct
{
    bool live;
    bool disabled;
    uint32_t type;
    uint64_t config;
    uint64_t reading[3];
} events[128];

static void *fixture_alloc(size_t count, size_t size)
{
    void *result = calloc(count, size);
    CHECK(result != NULL);
    allocations++;
    return result;
}

static void fixture_free(void *pointer)
{
    CHECK(pointer != NULL && allocations > 0);
    allocations--;
    free(pointer);
    errno = EBADF;
}

zend_object *zend_throw_exception(zend_class_entry *ce, const char *message, zend_long code)
{
    (void) message;
    CHECK(thrown == NULL);
    thrown = ce;
    thrown_code = code;
    fds_at_exception = live_fds;
    return NULL;
}

zend_object *zend_throw_exception_ex(zend_class_entry *ce, zend_long code, const char *format, ...)
{
    return zend_throw_exception(ce, format, code);
}

static long fixture_syscall(long number, ...)
{
    if (number == SYS_gettid) {
        return owner_tid;
    }
    CHECK(number == SYS_perf_event_open);
    perf_open_calls++;
    va_list args;
    va_start(args, number);
    const struct perf_event_attr *attr = va_arg(args, const struct perf_event_attr *);
    CHECK(va_arg(args, int) == 0);
    CHECK(va_arg(args, int) == -1);
    CHECK(va_arg(args, int) == -1);
    CHECK(va_arg(args, unsigned long) == PERF_FLAG_FD_CLOEXEC);
    va_end(args);
    CHECK(attr->size == sizeof(*attr));
    CHECK(!attr->inherit && !attr->exclude_user && !attr->exclude_kernel && attr->exclude_hv);
    CHECK(attr->read_format == (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING));
    if ((int) attr->config == fail_open_config && (int) attr->type == fail_open_type) {
        errno = fail_errno;
        return -1;
    }
    CHECK(next_fd < 128);
    int fd = next_fd++;
    events[fd].live = true;
    events[fd].disabled = attr->disabled;
    events[fd].type = attr->type;
    events[fd].config = attr->config;
    events[fd].reading[0] = 10;
    events[fd].reading[1] = 100;
    events[fd].reading[2] = 100;
    live_fds++;
    return fd;
}

static int fixture_close(int fd)
{
    CHECK(fd >= 10 && fd < next_fd && events[fd].live);
    events[fd].live = false;
    live_fds--;
    errno = EBADF;
    return 0;
}

static ssize_t fixture_read(int fd, void *buffer, size_t length)
{
    CHECK(fd >= 10 && fd < next_fd && events[fd].live);
    CHECK(length == sizeof(events[fd].reading));
    read_calls++;
    if (fd == interrupted_read_fd && interrupted_read_count > 0) {
        interrupted_read_count--;
        errno = EINTR;
        return -1;
    }
    if (fd == fail_read_fd) {
        errno = EIO;
        return -1;
    }
    memcpy(buffer, events[fd].reading, length);
    return fd == short_read_fd ? (ssize_t) length - 1 : (ssize_t) length;
}

#undef ecalloc
#define ecalloc(count, size) fixture_alloc((count), (size))
#undef efree
#define efree(pointer) fixture_free(pointer)
#define syscall fixture_syscall
#define close fixture_close
#define read fixture_read
#include "../../src/linux/sampler.c"
#undef syscall
#undef close
#undef read

int main(void)
{
    const uint32_t all = (1U << PERFIDIOUS_METRIC_COUNT) - 1;
    uint32_t supported;
    uint64_t scaled;
    struct perfidious_platform_sampler *sampler = NULL;
    struct perfidious_sampler_snapshot snapshot;

    CHECK(perfidious_scale_uint64(UINT64_MAX, 2, 2, &scaled));
    CHECK(scaled == UINT64_MAX);
    CHECK(perfidious_scale_uint64(1, 3, 2, &scaled));
    CHECK(scaled == 1);
    CHECK(!perfidious_scale_uint64(UINT64_MAX, UINT64_MAX, UINT64_MAX - 1, &scaled));

    int probe_first = next_fd;
    CHECK(perfidious_platform_sampler_supported_metrics(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported) == SUCCESS);
    CHECK(supported == all && live_fds == 0);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(events[probe_first + i].disabled);
    }
    size_t calls_before_process_probe = perf_open_calls;
    CHECK(perfidious_platform_sampler_supported_metrics(all, PERFIDIOUS_SCOPE_CURRENT_PROCESS, &supported) == SUCCESS);
    CHECK(supported == 0 && live_fds == 0 && perf_open_calls == calls_before_process_probe);

    fail_open_type = PERF_TYPE_HARDWARE;
    fail_open_config = PERF_COUNT_HW_INSTRUCTIONS;
    fail_errno = EACCES;
    size_t calls_before_subset_probe = perf_open_calls;
    CHECK(
        perfidious_platform_sampler_supported_metrics(
            PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported
        ) == SUCCESS
    );
    CHECK(
        supported == PERFIDIOUS_METRIC_CPU_TIME_MASK && live_fds == 0 &&
        perf_open_calls == calls_before_subset_probe + 1
    );
    const int unavailable_errors[] = {ENOENT, ENODEV, EOPNOTSUPP, EINVAL};
    for (size_t i = 0; i < sizeof(unavailable_errors) / sizeof(unavailable_errors[0]); ++i) {
        fail_errno = unavailable_errors[i];
        CHECK(
            perfidious_platform_sampler_supported_metrics(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported) == SUCCESS
        );
        CHECK(supported == (all & ~PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_INSTRUCTIONS)) && live_fds == 0);
    }
    fail_errno = EACCES;
    CHECK(perfidious_platform_sampler_supported_metrics(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &supported) == FAILURE);
    CHECK(thrown == &io_class && thrown_code == EACCES && live_fds == 0);
    thrown = NULL;
    sampler = (struct perfidious_platform_sampler *) (uintptr_t) 1;
    CHECK(perfidious_platform_sampler_open(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) == FAILURE);
    CHECK(sampler == (struct perfidious_platform_sampler *) (uintptr_t) 1);
    CHECK(allocations == 0 && live_fds == 0 && fds_at_exception == 0);
    CHECK(thrown == &io_class && thrown_code == EACCES);
    thrown = NULL;
    sampler = NULL;
    fail_open_config = -1;

    int first = next_fd;
    CHECK(perfidious_platform_sampler_open(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) == SUCCESS);
    CHECK(sampler != NULL && allocations == 1 && live_fds == 5);
    CHECK(events[first].type == PERF_TYPE_SOFTWARE && events[first].config == PERF_COUNT_SW_TASK_CLOCK);
    CHECK(events[first + 1].config == PERF_COUNT_SW_PAGE_FAULTS);
    CHECK(events[first + 2].config == PERF_COUNT_SW_CONTEXT_SWITCHES);
    CHECK(events[first + 3].type == PERF_TYPE_HARDWARE && events[first + 3].config == PERF_COUNT_HW_CPU_CYCLES);
    CHECK(events[first + 4].type == PERF_TYPE_HARDWARE && events[first + 4].config == PERF_COUNT_HW_INSTRUCTIONS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(!events[first + i].disabled);
    }
    interrupted_read_fd = first;
    interrupted_read_count = 2;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    CHECK(interrupted_read_count == 0);
    for (int i = 0; i < 5; ++i) {
        CHECK(snapshot.values[i] == 10);
        events[first + i].reading[0] = 30;
        events[first + i].reading[1] = 300;
        events[first + i].reading[2] = 200;
    }
    fail_read_fd = first + 4;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    CHECK(thrown == &io_class);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == UINT64_C(0xa5a5a5a5a5a5a5a5));
    }
    thrown = NULL;
    fail_read_fd = -1;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < 5; ++i) {
        CHECK(snapshot.values[i] == 50); /* 10 + (30 - 10) * (300 - 100) / (200 - 100). */
        events[first + i].reading[0] = 40;
        events[first + i].reading[1] = 400;
        events[first + i].reading[2] = 300;
    }
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 60); /* A changing ratio must not rescale earlier observations. */
    }

    size_t reads_before_wrong_thread = read_calls;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    owner_tid++;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE);
    CHECK(thrown == &thread_class && read_calls == reads_before_wrong_thread);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == UINT64_C(0xa5a5a5a5a5a5a5a5));
    }
    thrown = NULL;
    owner_tid--;

    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        events[first + i].reading[0] = 50;
        events[first + i].reading[1] = 500;
        events[first + i].reading[2] = 400;
    }
    short_read_fd = first + 4;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE && thrown == &io_class);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == UINT64_C(0xa5a5a5a5a5a5a5a5));
    }
    thrown = NULL;
    short_read_fd = -1;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 70);
        events[first + i].reading[0] = 60;
        events[first + i].reading[1] = 600;
        events[first + i].reading[2] = 500;
    }
    events[first + 4].reading[0] = 49;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE && thrown == &io_class);
    thrown = NULL;
    events[first + 4].reading[0] = 60;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 80);
        events[first + i].reading[0] = 70;
        events[first + i].reading[1] = 700;
        events[first + i].reading[2] = 600;
    }
    events[first + 4].reading[2] = 800;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE && thrown == &io_class);
    thrown = NULL;
    events[first + 4].reading[2] = 600;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 90);
        events[first + i].reading[0] = 80;
        events[first + i].reading[1] = 800;
        events[first + i].reading[2] = 700;
    }
    events[first + 4].reading[0] = 70;
    events[first + 4].reading[2] = 600;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE && thrown == &io_class);
    thrown = NULL;
    events[first + 4].reading[0] = 80;
    events[first + 4].reading[2] = 700;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 100);
        events[first + i].reading[0] = 90;
        events[first + i].reading[1] = 900;
        events[first + i].reading[2] = 800;
    }
    events[first + 4].reading[0] = UINT64_MAX;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == FAILURE && thrown == &overflow_class);
    thrown = NULL;
    events[first + 4].reading[0] = 90;
    CHECK(perfidious_platform_sampler_read(sampler, &snapshot) == SUCCESS);
    for (int i = 0; i < PERFIDIOUS_METRIC_COUNT; ++i) {
        CHECK(snapshot.values[i] == 110);
    }
    perfidious_platform_sampler_close(sampler);
    CHECK(allocations == 0 && live_fds == 0);

    struct perfidious_platform_sampler *survivor = NULL;
    CHECK(
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &survivor) ==
        SUCCESS
    );
    int survivor_fd = survivor->counters[PERFIDIOUS_METRIC_CPU_TIME].fd;
    fail_open_type = PERF_TYPE_HARDWARE;
    fail_open_config = PERF_COUNT_HW_INSTRUCTIONS;
    fail_errno = EMFILE;
    struct perfidious_platform_sampler *rejected = (struct perfidious_platform_sampler *) (uintptr_t) 1;
    CHECK(perfidious_platform_sampler_open(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &rejected) == FAILURE);
    CHECK(rejected == (struct perfidious_platform_sampler *) (uintptr_t) 1);
    CHECK(thrown == &io_class && thrown_code == EMFILE && fds_at_exception == 1);
    CHECK(allocations == 1 && live_fds == 1 && events[survivor_fd].live);
    thrown = NULL;
    fail_open_config = -1;
    CHECK(perfidious_platform_sampler_read(survivor, &snapshot) == SUCCESS);
    CHECK(snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == 10);

    struct perfidious_platform_sampler *second = NULL;
    CHECK(
        perfidious_platform_sampler_open(PERFIDIOUS_METRIC_CPU_TIME_MASK, PERFIDIOUS_SCOPE_CURRENT_THREAD, &second) ==
        SUCCESS
    );
    int second_fd = second->counters[PERFIDIOUS_METRIC_CPU_TIME].fd;
    CHECK(perfidious_platform_sampler_read(second, &snapshot) == SUCCESS);
    perfidious_platform_sampler_close(survivor);
    CHECK(allocations == 1 && live_fds == 1 && events[second_fd].live);
    events[second_fd].reading[0] = 25;
    events[second_fd].reading[1] = 250;
    events[second_fd].reading[2] = 250;
    CHECK(perfidious_platform_sampler_read(second, &snapshot) == SUCCESS);
    CHECK(snapshot.values[PERFIDIOUS_METRIC_CPU_TIME] == 25);
    perfidious_platform_sampler_close(second);
    CHECK(allocations == 0 && live_fds == 0);
    puts("Linux perf sampler mapping, scaling, failures and ownership passed");
    return 0;
}
