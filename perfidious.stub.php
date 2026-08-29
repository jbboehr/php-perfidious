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

namespace Perfidious;

const VERSION = "0.2.0";

const OVERFLOW_THROW = 0;
const OVERFLOW_WARN = 1;
const OVERFLOW_SATURATE = 2;
const OVERFLOW_WRAP = 3;

/**
 * @throws PmuNotFoundException
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
function get_pmu_info(int $pmu): PmuInfo
{
}

/**
 * @phpstan-return ?Handle<list<string>>
 */
function global_handle(): ?Handle
{
}

/**
 * @return list<PmuInfo>
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
function list_pmus(): array
{
}

/**
 * @return list<PmuEventInfo>
 * @throws PmuNotFoundException|PmuEventNotFoundException
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_event_info.html
 */
function list_pmu_events(int $pmu): array
{
}

/**
 * @param list<string> $event_names a list of libpfm event names, see list_pmu_events
 * @throws PmuEventNotFoundException|IOException|OverflowException
 *
 * @phpstan-template T of string
 * @phpstan-param list<T> $event_names
 * @phpstan-return Handle<list<T>>
 */
function open(array $event_names, int $pid = 0, int $cpu = -1): Handle
{
}

/**
 * @phpstan-return ?Handle<list<string>>
 */
function request_handle(): ?Handle
{
}

interface ExceptionInterface
{
}

final class IOException extends \RuntimeException implements ExceptionInterface
{
}

final class OverflowException extends \OverflowException implements ExceptionInterface
{
}

final class PmuNotFoundException extends \InvalidArgumentException implements ExceptionInterface
{
}

final class PmuEventNotFoundException extends \InvalidArgumentException implements ExceptionInterface
{
}

final class UnsupportedMetricException extends \RuntimeException implements ExceptionInterface
{
}

enum Scope: string
{
    case CurrentProcess = 'current-process';
    case CurrentThread = 'current-thread';
}

enum Metric: string
{
    case CpuTime = 'cpu-time';
    case PageFaults = 'page-faults';
    case ContextSwitches = 'context-switches';
    case CpuCycles = 'cpu-cycles';
    case Instructions = 'instructions';
}

final class Sampler
{
    private function __construct()
    {
    }

    /** @param non-empty-list<Metric> $metrics */
    public static function open(array $metrics, Scope $scope = Scope::CurrentProcess): self
    {
    }

    /** @return non-empty-list<Metric> */
    public function metrics(): array
    {
    }

    public function read(): Sample
    {
    }

    public function close(): void
    {
    }
}

final class Sample
{
    private function __construct()
    {
    }

    public function value(Metric $metric): int
    {
    }

    public function since(self $earlier): SampleDelta
    {
    }
}

final class SampleDelta
{
    private function __construct()
    {
    }

    public readonly int $elapsedTimeNs;

    public function value(Metric $metric): int
    {
    }
}

/**
 * @phpstan-template T of list<string>
 */
final class Handle
{
    /**
     * @return $this
     * @throws IOException
     */
    final public function enable(): self
    {
    }

    /**
     * @return $this
     * @throws IOException
     */
    final public function disable(): self
    {
    }

    /**
     * Get a raw byte stream backed by a duplicate of the handle's file descriptor
     *
     * @note the returned stream owns an independent copy of the file descriptor, so closing it
     *       does not affect this handle or subsequent calls to read()
     * @return resource
     */
    final public function rawStream()
    {
    }

    /**
     * @note If perfidious.overflow_mode is set to Perfidious\OVERFLOW_WARN, this method can return null, despite its
     *       typehint. If perfidious.overflow_mode is set to any value other than Perfidious\OVERFLOW_THROW, this
     *       method will *not* throw an OverflowException.
     *
     * @return ReadResult
     * @throws OverflowException|IOException
     *
     * @phpstan-return ReadResult<T>
     */
    final public function read(): ReadResult
    {
    }

    /**
     * @note If perfidious.overflow_mode is set to Perfidious\OVERFLOW_WARN, this method can return null, despite its
     *       typehint. If perfidious.overflow_mode is set to any value other than Perfidious\OVERFLOW_THROW, this
     *       method will *not* throw an OverflowException.
     *
     * @return array
     * @throws OverflowException|IOException
     *
     * @phpstan-return array<value-of<T>, int>
     */
    final public function readArray(): array
    {
    }

    /**
     * @return $this
     * @throws IOException
     */
    final public function reset(): self
    {
    }
}

/**
 * @phpstan-template T of list<string>
 */
final class ReadResult
{
    public readonly int $timeEnabled;
    public readonly int $timeRunning;
    /**
     * @var array<string, int>
     * @phpstan-var array<value-of<T>, int>
     */
    public readonly array $values;
}

/**
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
final class PmuInfo
{
    /**
     * This is the symbolic name of the PMU. This name can be used as a prefix in an event string.
     */
    public string $name;
    public string $desc;
    /**
     * This is the unique PMU identification code. It is identical to the value passed in pmu and it provided only for
     * completeness.
     */
    public int $pmu;
    public int $type;
    /**
     * This is the number of available events for this PMU model based on the host processor. It is only valid is the
     * is_present field is set to true.
     */
    public int $nevents;
    /**
     * This field is set to true if the PMU model has been detected on the host system.
     */
    public bool $is_present;
}

/**
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_event_info.html
 */
final class PmuEventInfo
{
    public string $name;
    public string $desc;
    /**
     * Certain events may be just variations of actual events. They may be provided as handy shortcuts to avoid
     * supplying a long list of attributes. For those events, this field is not NULL and contains the complete
     * equivalent event string.
     */
    public ?string $equiv;
    /**
     * This is the ID of the PMU model this event belongs to.
     */
    public int $pmu;
    /**
     * This field is set to true if the PMU model has been detected on the host system.
     */
    public bool $is_present;
}

namespace Perfidious\Darwin;

/**
 * Return cumulative resource usage for the current process.
 *
 * CPU times are reported in nanoseconds. Hardware cycle and instruction counts may be zero when
 * the kernel cannot collect them, including on some virtualized systems.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://developer.apple.com/documentation/kernel/rusage_info_v4
 */
function get_current_process_resource_usage(): ProcessResourceUsage
{
}

/**
 * Return cumulative resource usage for the current thread.
 *
 * CPU times are reported in nanoseconds. Hardware cycle and instruction counts may be zero when
 * the kernel cannot collect them, including on macOS before 12.4 and on some virtualized systems.
 *
 * @throws \Perfidious\IOException|\Perfidious\OverflowException
 * @see https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/resource_private.h
 */
function get_current_thread_resource_usage(): ThreadResourceUsage
{
}

final class ProcessResourceUsage
{
    private function __construct()
    {
    }

    public readonly int $userTimeNs;
    public readonly int $systemTimeNs;
    public readonly int $minorPageFaultCount;
    public readonly int $majorPageFaultCount;
    public readonly int $voluntaryContextSwitchCount;
    public readonly int $involuntaryContextSwitchCount;
    public readonly int $instructionCount;
    public readonly int $cycleCount;
}

final class ThreadResourceUsage
{
    private function __construct()
    {
    }

    public readonly int $userTimeNs;
    public readonly int $systemTimeNs;
    public readonly int $instructionCount;
    public readonly int $cycleCount;
}

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
 * @throws \Perfidious\IOException
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
     * @throws \Perfidious\IOException|\Perfidious\OverflowException
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
