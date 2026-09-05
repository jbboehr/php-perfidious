#ifndef PERFIDIOUS_TEST_MACH_H
#define PERFIDIOUS_TEST_MACH_H

/* Minimal native boundary for compiling the real Darwin backend on Linux. */
#include <stdint.h>
#include <pthread.h>

typedef int kern_return_t;
typedef unsigned int thread_t;
typedef unsigned int mach_msg_type_number_t;
typedef int *thread_info_t;
typedef struct
{
    int seconds;
    int microseconds;
} time_value_t;
typedef struct
{
    time_value_t user_time;
    time_value_t system_time;
} thread_basic_info_data_t;

#define KERN_SUCCESS 0
#define KERN_FAILURE 5
#define THREAD_BASIC_INFO 3
#define THREAD_BASIC_INFO_COUNT (sizeof(thread_basic_info_data_t) / sizeof(int))

thread_t mach_thread_self(void);
thread_t mach_task_self(void);
kern_return_t mach_port_deallocate(thread_t task, thread_t thread);
kern_return_t thread_info(thread_t thread, int flavor, thread_info_t info, mach_msg_type_number_t *count);
int pthread_threadid_np(void *thread, uint64_t *thread_id);

#endif
