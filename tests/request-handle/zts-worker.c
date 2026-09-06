/* SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception */

#include <php.h>
#include <main/php_main.h>
#include <main/php_network.h>
#include <main/SAPI.h>
#include <Zend/zend_exceptions.h>
#include <dirent.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#ifndef ZTS
#error This fixture requires a ZTS PHP build
#endif

ZEND_TSRMLS_CACHE_DEFINE()

struct worker
{
    int index;
    int request;
    uint64_t group_id;
    uint64_t event_id;
    const char *script;
};

static struct worker workers[2];
static _Thread_local struct worker *current_worker;
static pthread_barrier_t rendezvous;
static pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t finish_condition = PTHREAD_COND_INITIALIZER;
static bool first_finished;

static void fail(const char *format, ...)
{
    va_list args;
    fprintf(
        stderr,
        "ZTS worker %d request %d: ",
        current_worker ? current_worker->index : -1,
        current_worker ? current_worker->request : -1
    );
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    // A failed worker must not leave its peer blocked at a barrier.
    _Exit(1);
}

static void synchronize(void)
{
    int result = pthread_barrier_wait(&rendezvous);
    if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
        fail("barrier failed: %s", strerror(result));
    }
}

static uint64_t thread_cpu_time(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0) {
        fail("clock_gettime failed: %s", strerror(errno));
    }
    return (uint64_t) now.tv_sec * UINT64_C(1000000000) + (uint64_t) now.tv_nsec;
}

// Workers cannot open or close perf descriptors during these observation phases.
// No duplicate stream remains open at these observation points.
static int perf_descriptors(uint64_t wanted_id, int *found_fd)
{
    DIR *directory = opendir("/proc/self/fd");
    if (directory == NULL) {
        fail("could not inspect descriptors: %s", strerror(errno));
    }
    int count = 0;
    *found_fd = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        char target[128];
        ssize_t length = readlinkat(dirfd(directory), entry->d_name, target, sizeof(target) - 1);
        if (length < 0) {
            continue;
        }
        target[length] = '\0';
        if (strcmp(target, "anon_inode:[perf_event]") != 0) {
            continue;
        }
        int fd = atoi(entry->d_name);
        uint64_t id;
        if (ioctl(fd, PERF_EVENT_IOC_ID, &id) != 0) {
            fail("could not read perf event identity: %s", strerror(errno));
        }
        ++count;
        if (id == wanted_id) {
            *found_fd = fd;
        }
    }
    closedir(directory);
    return count;
}

static void check_shutdown_counter(void)
{
    int fd;
    perf_descriptors(current_worker->group_id, &fd);
    if (fd < 0) {
        fail("request shutdown closed a persistent group");
    }
    uint64_t data[16];
    ssize_t length = read(fd, data, sizeof(data));
    if (length < 3 * (ssize_t) sizeof(uint64_t) || data[0] > 6 ||
        length != (ssize_t) ((3 + 2 * data[0]) * sizeof(uint64_t))) {
        fail("invalid native group read after request shutdown");
    }
    for (uint64_t i = 0; i < data[0]; ++i) {
        if (data[4 + 2 * i] == current_worker->event_id) {
            if (data[3 + 2 * i] != 0) {
                fail("request shutdown did not reset its counter");
            }
            return;
        }
    }
    fail("request shutdown lost its metric");
}

static void execute_request(void)
{
    if (php_request_startup() != SUCCESS) {
        fail("request startup failed");
    }
    zend_file_handle file;
    zend_stream_init_filename(&file, current_worker->script);
    if (!php_execute_script(&file) || EG(exception) || EG(exit_status) != 0) {
        fail("worker script failed");
    }
    zend_destroy_file_handle(&file);
    php_request_shutdown(NULL);
}

static void *run_worker(void *argument)
{
    current_worker = argument;
    (void) ts_resource(0);
    ZEND_TSRMLS_CACHE_UPDATE();
    SG(options) |= SAPI_OPTION_NO_CHDIR;

    for (current_worker->request = 0; current_worker->request < 2; ++current_worker->request) {
        execute_request();
        synchronize();
        // Both workers are outside a request; no PHP allocations or streams survive.
        check_shutdown_counter();
        synchronize();
    }

    if (current_worker->index == 0) {
        ts_free_thread();
        int fd;
        perf_descriptors(current_worker->group_id, &fd);
        if (fd >= 0) {
            fail("thread destruction retained its perf group");
        }
        perf_descriptors(workers[1].group_id, &fd);
        if (fd < 0) {
            fail("thread destruction closed the surviving worker's group");
        }
        pthread_mutex_lock(&finish_mutex);
        first_finished = true;
        pthread_cond_signal(&finish_condition);
        pthread_mutex_unlock(&finish_mutex);
    } else {
        pthread_mutex_lock(&finish_mutex);
        while (!first_finished) {
            pthread_cond_wait(&finish_condition, &finish_mutex);
        }
        pthread_mutex_unlock(&finish_mutex);
        execute_request();
        check_shutdown_counter();
        ts_free_thread();
    }
    return NULL;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_run_arginfo, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, script, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_run)
{
    char *script;
    size_t length;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(script, length)
    ZEND_PARSE_PARAMETERS_END();
    int ignored;
    int baseline = perf_descriptors(0, &ignored);
    first_finished = false;
    if (pthread_barrier_init(&rendezvous, NULL, 2) != 0) {
        fail("could not initialize barrier");
    }
    pthread_t threads[2];
    for (int i = 0; i < 2; ++i) {
        workers[i] = (struct worker) {.index = i, .script = script};
        int result = pthread_create(&threads[i], NULL, run_worker, &workers[i]);
        if (result != 0) {
            fail("could not create worker: %s", strerror(result));
        }
    }
    for (int i = 0; i < 2; ++i) {
        int result = pthread_join(threads[i], NULL);
        if (result != 0) {
            fail("could not join worker: %s", strerror(result));
        }
    }
    pthread_barrier_destroy(&rendezvous);
    if (perf_descriptors(0, &ignored) != baseline) {
        fail("thread teardown did not restore the perf descriptor baseline");
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_context_arginfo, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_context)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
    add_next_index_long(return_value, current_worker->index);
    add_next_index_long(return_value, current_worker->request);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_void_arginfo, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_barrier)
{
    ZEND_PARSE_PARAMETERS_NONE();
    synchronize();
}

static PHP_FUNCTION(fixture_zts_work)
{
    ZEND_PARSE_PARAMETERS_NONE();
    uint64_t deadline = thread_cpu_time() + UINT64_C(40000000);
    volatile uint64_t value = 0;
    while (thread_cpu_time() < deadline) {
        for (unsigned int i = 0; i < 10000; ++i) {
            value += i;
        }
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_clock_arginfo, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_cpu_time)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) thread_cpu_time());
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_id_arginfo, 0, 1, IS_LONG, 0)
    ZEND_ARG_INFO(0, stream)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_event_id)
{
    zval *resource;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(resource)
    ZEND_PARSE_PARAMETERS_END();
    php_stream *stream;
    php_stream_from_zval(stream, resource);
    php_socket_t fd;
    uint64_t id;
    if (php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void *) &fd, 1) != SUCCESS ||
        ioctl(fd, PERF_EVENT_IOC_ID, &id) != 0 || id > ZEND_LONG_MAX) {
        fail("could not read stream event identity");
    }
    RETURN_LONG((zend_long) id);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(fixture_observe_arginfo, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, groupId, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, eventId, IS_LONG, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(fixture_zts_observe)
{
    zend_long group_id, event_id;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(group_id)
        Z_PARAM_LONG(event_id)
    ZEND_PARSE_PARAMETERS_END();
    if (current_worker->request == 0) {
        current_worker->group_id = (uint64_t) group_id;
        current_worker->event_id = (uint64_t) event_id;
    } else if (current_worker->group_id != (uint64_t) group_id || current_worker->event_id != (uint64_t) event_id) {
        fail("later request did not reuse its native events");
    }
    if (current_worker->request < 2) {
        synchronize();
        if (workers[0].group_id == workers[1].group_id || workers[0].event_id == workers[1].event_id) {
            fail("workers share native events");
        }
    }
}

static const zend_function_entry fixture_functions[] = {
    PHP_FE(fixture_zts_run, fixture_run_arginfo) PHP_FE(fixture_zts_context, fixture_context_arginfo)
        PHP_FE(fixture_zts_barrier, fixture_void_arginfo) PHP_FE(fixture_zts_work, fixture_void_arginfo)
            PHP_FE(fixture_zts_cpu_time, fixture_clock_arginfo) PHP_FE(fixture_zts_event_id, fixture_id_arginfo)
                PHP_FE(fixture_zts_observe, fixture_observe_arginfo) PHP_FE_END
};

zend_module_entry zts_fixture_module_entry = {
    STANDARD_MODULE_HEADER,
    "perfidious_zts_fixture",
    fixture_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "0",
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(zts_fixture)
