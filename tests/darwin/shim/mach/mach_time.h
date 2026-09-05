#ifndef PERFIDIOUS_TEST_MACH_TIME_H
#define PERFIDIOUS_TEST_MACH_TIME_H

#include "mach.h"

typedef struct
{
    uint32_t numer;
    uint32_t denom;
} mach_timebase_info_data_t;
kern_return_t mach_timebase_info(mach_timebase_info_data_t *info);

#endif
