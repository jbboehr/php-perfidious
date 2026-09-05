#ifndef PERFIDIOUS_TEST_LIBPROC_H
#define PERFIDIOUS_TEST_LIBPROC_H

#ifdef __APPLE__

#include_next <libproc.h>

#else

#include <stdint.h>

#define RUSAGE_INFO_V4 4

typedef void *rusage_info_t;

struct rusage_info_v4
{
    uint64_t ri_user_time;
    uint64_t ri_system_time;
    uint64_t ri_cycles;
    uint64_t ri_instructions;
};

int proc_pid_rusage(int pid, int flavor, rusage_info_t *buffer);

#endif /* __APPLE__ */

#endif /* PERFIDIOUS_TEST_LIBPROC_H */
