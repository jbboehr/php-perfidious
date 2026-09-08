/* Exercise the production backend with real user-only events on restricted Linux hosts. */
#include <errno.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "main/php.h"
#include "Zend/zend_exceptions.h"
#include "../../src/sampler.h"

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "Live perf fixture failed at line %d: %s\n", __LINE__, #condition);                        \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

static zend_class_entry io_class, overflow_class, thread_class;
zend_class_entry *perfidious_io_exception_ce = &io_class;
zend_class_entry *perfidious_overflow_exception_ce = &overflow_class;
zend_class_entry *perfidious_wrong_thread_exception_ce = &thread_class;
static zend_class_entry *thrown;

zend_object *zend_throw_exception(zend_class_entry *ce, const char *message, zend_long code)
{
    (void) message;
    (void) code;
    CHECK(thrown == NULL);
    thrown = ce;
    return NULL;
}
zend_object *zend_throw_exception_ex(zend_class_entry *ce, zend_long code, const char *format, ...)
{
    return zend_throw_exception(ce, format, code);
}

static long user_only_syscall(long number, ...)
{
    if (number == SYS_gettid) {
        return syscall(SYS_gettid);
    }
    CHECK(number == SYS_perf_event_open);
    va_list args;
    va_start(args, number);
    struct perf_event_attr attr = *va_arg(args, const struct perf_event_attr *);
    int pid = va_arg(args, int);
    int cpu = va_arg(args, int);
    int group = va_arg(args, int);
    unsigned long flags = va_arg(args, unsigned long);
    va_end(args);
    /* Only the privilege filter differs from production; no kernel result is fabricated. */
    attr.exclude_kernel = 1;
    return syscall(number, &attr, pid, cpu, group, flags);
}

#undef ecalloc
#define ecalloc(count, size) calloc((count), (size))
#undef efree
#define efree(pointer) free(pointer)
#define syscall user_only_syscall
#include "../../src/linux/sampler.c"
#undef syscall

static void work(void)
{
    struct timespec start, now;
    CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start) == 0);
    volatile uint64_t value = 1;
    do {
        for (int i = 0; i < 10000; ++i) {
            value = value * 1664525 + 1013904223;
        }
        CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) == 0);
    } while ((now.tv_sec - start.tv_sec) * INT64_C(1000000000) + now.tv_nsec - start.tv_nsec < 100000000);
}

static struct perfidious_platform_sampler *parent_sampler;
static uint32_t metrics;

static uint64_t thread_cpu_time(void)
{
    struct timespec time;
    CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0);
    return (uint64_t) time.tv_sec * UINT64_C(1000000000) + (uint64_t) time.tv_nsec;
}

static void check_parent_cpu_time(const char *phase, uint64_t measured, uint64_t expected)
{
    if (measured < expected - expected / 5 || measured > expected + expected / 5) {
        fprintf(
            stderr,
            "%s CPU time: perf=%" PRIu64 " ns, thread clock=%" PRIu64 " ns (allowed difference: 20%%)\n",
            phase,
            measured,
            expected
        );
        exit(1);
    }
}

static void *worker(void *unused)
{
    (void) unused;
    struct perfidious_sampler_snapshot before, after;
    struct perfidious_platform_sampler *sampler = NULL;
    CHECK(perfidious_platform_sampler_read(parent_sampler, &before) == FAILURE);
    CHECK(thrown == &thread_class);
    thrown = NULL;
    CHECK(perfidious_platform_sampler_open(metrics, PERFIDIOUS_SCOPE_CURRENT_THREAD, &sampler) == SUCCESS);
    CHECK(perfidious_platform_sampler_read(sampler, &before) == SUCCESS);
    void *pages = malloc(8 * 1024 * 1024);
    CHECK(pages != NULL);
    for (size_t i = 0; i < 8 * 1024 * 1024; i += 4096) {
        ((volatile char *) pages)[i] = 0x5a;
    }
    work();
    usleep(1000);
    CHECK(perfidious_platform_sampler_read(sampler, &after) == SUCCESS);
    for (int metric = 0; metric < PERFIDIOUS_METRIC_COUNT; ++metric) {
        CHECK(after.values[metric] >= before.values[metric]);
        uint64_t delta = after.values[metric] - before.values[metric];
        if (metrics & PERFIDIOUS_METRIC_MASK(metric)) {
            if (delta == 0) {
                fprintf(stderr, "Metric %d did not advance\n", metric);
            }
            CHECK(delta > 0);
        }
    }
    free(pages);
    perfidious_platform_sampler_close(sampler);
    return NULL;
}

int main(void)
{
    /* Context-switch events occur in the kernel and cannot advance with this user-only filter. */
    uint32_t all = ((1U << PERFIDIOUS_METRIC_COUNT) - 1) & ~PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK;
    CHECK(perfidious_platform_sampler_supported_metrics(all, PERFIDIOUS_SCOPE_CURRENT_THREAD, &metrics) == SUCCESS);
    uint32_t software = PERFIDIOUS_METRIC_CPU_TIME_MASK | PERFIDIOUS_METRIC_PAGE_FAULTS_MASK;
    CHECK((metrics & software) == software);
    CHECK(perfidious_platform_sampler_open(metrics, PERFIDIOUS_SCOPE_CURRENT_THREAD, &parent_sampler) == SUCCESS);
    struct perfidious_sampler_snapshot before, after;
    uint64_t before_thread = thread_cpu_time();
    CHECK(perfidious_platform_sampler_read(parent_sampler, &before) == SUCCESS);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, worker, NULL) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
    /* Include thread setup in the CPU-clock comparison; parent work limits sampling jitter. */
    work();
    CHECK(perfidious_platform_sampler_read(parent_sampler, &after) == SUCCESS);
    uint64_t parent_cpu = thread_cpu_time() - before_thread;
    uint64_t measured = after.values[PERFIDIOUS_METRIC_CPU_TIME] - before.values[PERFIDIOUS_METRIC_CPU_TIME];
    check_parent_cpu_time("Thread isolation", measured, parent_cpu);
    uint64_t before_child = thread_cpu_time();
    CHECK(perfidious_platform_sampler_read(parent_sampler, &after) == SUCCESS);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        struct perfidious_sampler_snapshot inherited;
        CHECK(perfidious_platform_sampler_read(parent_sampler, &inherited) == FAILURE);
        CHECK(thrown == &thread_class);
        perfidious_platform_sampler_close(parent_sampler);
        work();
        _exit(0);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    /* Parent work keeps accounting jitter small relative to the measured interval. */
    work();
    CHECK(perfidious_platform_sampler_read(parent_sampler, &before) == SUCCESS);
    parent_cpu = thread_cpu_time() - before_child;
    measured = before.values[PERFIDIOUS_METRIC_CPU_TIME] - after.values[PERFIDIOUS_METRIC_CPU_TIME];
    check_parent_cpu_time("Fork isolation", measured, parent_cpu);
    perfidious_platform_sampler_close(parent_sampler);
    CHECK(thrown == NULL);
    puts("Live user-only perf counters and thread isolation passed");
    return 0;
}
