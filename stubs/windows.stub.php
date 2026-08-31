<?php
/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3,
 * as published by the Free Software Foundation, together with the Romic
 * Exception (an additional permission under section 7 of that license).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * and the Romic Exception along with this program.  If not, see
 * <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
 */

namespace Perfidious\Windows;

/**
 * Return the current process's cumulative CPU cycle count.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryprocesscycletime
 */
function query_current_process_cycle_time(): int
{
}

/**
 * Return the current thread's cumulative CPU cycle count.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime
 */
function query_current_thread_cycle_time(): int
{
}

/**
 * Return timing information for the current process.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes
 */
function get_current_process_times(): ProcessTimes
{
}

/**
 * Return timing information for the current thread.
 *
 * The native exit timestamp is omitted because the current thread is necessarily still running.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes
 */
function get_current_thread_times(): ThreadTimes
{
}

/**
 * Return PROCESS_MEMORY_COUNTERS_EX values for the current process. Memory sizes are in bytes.
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo
 */
function get_current_process_memory_info(): ProcessMemoryInfo
{
}

/**
 * Enable profiling for the current thread.
 *
 * Hardware counters are a bitmask of up to 16 globally configured counter indices. Configuring
 * those counters requires a kernel driver; an unconfigured requested counter reads as zero.
 *
 * @throws \Perfidious\ResourceBusyException|\Perfidious\IOException
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-enablethreadprofiling
 */
function enable_current_thread_profiling(int $hardwareCounterMask = 0): ThreadProfile
{
}

final class ProcessTimes
{
    private function __construct()
    {
    }

    /**
     * The process creation timestamp as 100-nanosecond intervals since January 1, 1601 UTC.
     */
    public readonly int $creationTimeFiletime;

    /**
     * Time spent by the process in kernel mode, in 100-nanosecond units.
     */
    public readonly int $kernelTime100ns;

    /**
     * Time spent by the process in user mode, in 100-nanosecond units.
     */
    public readonly int $userTime100ns;
}

final class ThreadTimes
{
    private function __construct()
    {
    }

    /**
     * The thread creation timestamp as 100-nanosecond intervals since January 1, 1601 UTC.
     */
    public readonly int $creationTimeFiletime;

    /**
     * Time spent by the thread in kernel mode, in 100-nanosecond units.
     */
    public readonly int $kernelTime100ns;

    /**
     * Time spent by the thread in user mode, in 100-nanosecond units.
     */
    public readonly int $userTime100ns;
}

final class ProcessMemoryInfo
{
    private function __construct()
    {
    }

    public readonly int $pageFaultCount;
    public readonly int $peakWorkingSetSize;
    public readonly int $workingSetSize;
    public readonly int $quotaPeakPagedPoolUsage;
    public readonly int $quotaPagedPoolUsage;
    public readonly int $quotaPeakNonPagedPoolUsage;
    public readonly int $quotaNonPagedPoolUsage;

    /**
     * The process commit charge in bytes; retained under the native PROCESS_MEMORY_COUNTERS_EX field name.
     */
    public readonly int $pagefileUsage;

    /**
     * The peak process commit charge in bytes; retained under the native PROCESS_MEMORY_COUNTERS_EX field name.
     */
    public readonly int $peakPagefileUsage;

    /**
     * The process's private committed memory in bytes.
     */
    public readonly int $privateUsage;
}

final class HardwareCounterSnapshot
{
    private function __construct()
    {
    }

    public readonly int $index;
    public readonly int $type;
    public readonly int $value;
}

final class ThreadProfileSnapshot
{
    private function __construct()
    {
    }

    /**
     * Context switches since profiling was enabled.
     */
    public readonly int $contextSwitchCount;

    /**
     * Wait reasons observed since the previous native read, encoded as a fixed-width hexadecimal bitmap.
     */
    public readonly string $waitReasonBitmapHex;

    /**
     * CPU cycles since the profiler's initial baseline sample.
     */
    public readonly int $cycleCount;

    /**
     * Reads Windows needed to obtain this consistent snapshot.
     */
    public readonly int $readRetryCount;

    /**
     * Number of hardware-counter array elements Windows reports as populated.
     */
    public readonly int $hardwareCounterCount;

    /**
     * Every requested counter keyed by its globally configured index. A value of zero may mean either
     * no events occurred or the requested index was not configured.
     *
     * @var array<int, HardwareCounterSnapshot>
     */
    public readonly array $hardwareCounters;
}

final class ThreadProfile
{
    private function __construct()
    {
    }

    /**
     * @throws \Perfidious\ClosedException|\Perfidious\IOException|\Perfidious\OverflowException
     * @see https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readthreadprofilingdata
     */
    final public function read(): ThreadProfileSnapshot
    {
    }

    /**
     * @throws \Perfidious\IOException
     */
    final public function close(): void
    {
    }
}
