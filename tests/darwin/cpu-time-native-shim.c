/* Test-only native calls for the Darwin backend, loaded in an isolated PHP process. */
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach_time.h>
#include <libproc.h>

static uint64_t setting(const char *name, uint64_t fallback)
{
    const char *value = getenv(name);
    return value != NULL ? strtoull(value, NULL, 10) : fallback;
}

int proc_pid_rusage(int pid, int flavor, rusage_info_t *buffer)
{
    (void) pid;
    if (flavor != RUSAGE_INFO_V4) {
        errno = EINVAL;
        return -1;
    }
    struct rusage_info_v4 *usage = (struct rusage_info_v4 *) buffer;
    memset(usage, 0, sizeof(*usage));
    usage->ri_user_time = setting("PERFIDIOUS_TEST_USER_TICKS", 18000000);
    usage->ri_system_time = setting("PERFIDIOUS_TEST_SYSTEM_TICKS", 6000000);
    usage->ri_cycles = 12;
    usage->ri_instructions = 34;
    return 0;
}

kern_return_t mach_timebase_info(mach_timebase_info_data_t *info)
{
    info->numer = (uint32_t) setting("PERFIDIOUS_TEST_NUMER", 125);
    info->denom = (uint32_t) setting("PERFIDIOUS_TEST_DENOM", 3);
    return setting("PERFIDIOUS_TEST_TIMEBASE_FAILURE", 0) ? KERN_FAILURE : KERN_SUCCESS;
}

static int thread_selfcounts(uint32_t kind, void *destination, size_t size)
{
    const uint64_t usage[] = {34, 12, 18000000, 6000000};
    if (kind != 3 || size != sizeof(usage)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(destination, usage, sizeof(usage));
    return 0;
}

/* Only the test module's dlsym calls are renamed at compile time. */
void *perfidious_test_dlsym(void *handle, const char *name)
{
    (void) handle;
    union
    {
        void *object;
        int (*function)(uint32_t, void *, size_t);
    } symbol = {0};
    if (strcmp(name, "thread_selfcounts") == 0 && setting("PERFIDIOUS_TEST_SELFCOUNTS", 0)) {
        symbol.function = thread_selfcounts;
    }
    return symbol.object;
}

int pthread_threadid_np(void *thread, uint64_t *thread_id)
{
    (void) thread;
    *thread_id = 42;
    return 0;
}

thread_t mach_thread_self(void)
{
    return 42;
}
thread_t mach_task_self(void)
{
    return 1;
}
kern_return_t mach_port_deallocate(thread_t task, thread_t thread)
{
    (void) task;
    (void) thread;
    return KERN_SUCCESS;
}
kern_return_t thread_info(thread_t thread, int flavor, thread_info_t info, mach_msg_type_number_t *count)
{
    (void) thread;
    if (flavor != THREAD_BASIC_INFO || *count != THREAD_BASIC_INFO_COUNT) {
        return KERN_FAILURE;
    }
    const thread_basic_info_data_t usage = {
        {2, 300},
        {4, 500}
    };
    memcpy(info, &usage, sizeof(usage));
    return KERN_SUCCESS;
}
