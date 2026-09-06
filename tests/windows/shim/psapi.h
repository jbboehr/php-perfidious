#ifndef PERFIDIOUS_TEST_PSAPI_H
#define PERFIDIOUS_TEST_PSAPI_H

#include "windows.h"

/* Only fields consumed by sampler.c are needed by this native-call fixture. */
typedef struct
{
    DWORD cb;
    DWORD PageFaultCount;
} PROCESS_MEMORY_COUNTERS_EX;
typedef PROCESS_MEMORY_COUNTERS_EX PROCESS_MEMORY_COUNTERS;

BOOL GetProcessMemoryInfo(HANDLE process, PROCESS_MEMORY_COUNTERS *memory, DWORD size);

#endif
