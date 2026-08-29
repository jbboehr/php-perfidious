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
