/* Test-only Windows API subset. This models values and ownership, not the SDK ABI. */
#ifndef PERFIDIOUS_TEST_WINDOWS_H
#define PERFIDIOUS_TEST_WINDOWS_H

#include <stddef.h>
#include <stdint.h>

typedef int BOOL;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t DWORD64;
typedef uint64_t ULONG64;
typedef void *HANDLE;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#define ERROR_SUCCESS 0
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_HANDLE 6
#define ERROR_INVALID_PARAMETER 87
#define ERROR_WMI_ALREADY_ENABLED 4206
#define DUPLICATE_SAME_ACCESS 2
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED UINT32_MAX
#define THREAD_PROFILING_FLAG_DISPATCH 1
#define READ_THREAD_PROFILING_FLAG_DISPATCHING 1
#define READ_THREAD_PROFILING_FLAG_HARDWARE_COUNTERS 2
#define PERFORMANCE_DATA_VERSION 1

typedef struct
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef union
{
    struct
    {
        DWORD LowPart;
        DWORD HighPart;
    };
    uint64_t QuadPart;
} ULARGE_INTEGER;

typedef struct
{
    WORD Size;
    BYTE Version;
    BYTE HwCountersCount;
    DWORD ContextSwitchCount;
    DWORD64 WaitReasonBitMap;
    DWORD64 CycleTime;
    DWORD RetryCount;
    DWORD Reserved;
} PERFORMANCE_DATA;

HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD GetCurrentThreadId(void);
DWORD GetLastError(void);
BOOL DuplicateHandle(
    HANDLE source_process,
    HANDLE source,
    HANDLE target_process,
    HANDLE *target,
    DWORD access,
    BOOL inherit,
    DWORD options
);
BOOL CloseHandle(HANDLE handle);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL GetProcessTimes(HANDLE handle, FILETIME *creation, FILETIME *exit_time, FILETIME *kernel, FILETIME *user);
BOOL GetThreadTimes(HANDLE handle, FILETIME *creation, FILETIME *exit_time, FILETIME *kernel, FILETIME *user);
BOOL QueryProcessCycleTime(HANDLE handle, ULONG64 *cycles);
DWORD EnableThreadProfiling(HANDLE thread, DWORD flags, DWORD64 mask, HANDLE *profile);
DWORD ReadThreadProfilingData(HANDLE profile, DWORD flags, PERFORMANCE_DATA *data);
DWORD DisableThreadProfiling(HANDLE profile);

#endif
