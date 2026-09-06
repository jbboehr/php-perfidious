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

/**
 * @throws PmuNotFoundException
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
function get_pmu_info(int $pmu): PmuInfo
{
}

/**
 * The event index must belong to the requested PMU.
 *
 * @throws PmuNotFoundException|PmuEventNotFoundException
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_event_info.html
 */
function get_pmu_event_info(int $pmu, int $idx): PmuEventInfo
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
 * @param int $pid process/thread selector, which must fit the native PID type
 * @param int $cpu -1 or a nonnegative CPU ID that fits a native int, with availability checked by the kernel
 * @throws PmuEventNotFoundException|IOException|OverflowException|\ValueError
 *
 * @phpstan-template T of string
 * @phpstan-param list<T> $event_names
 * @phpstan-return Handle<list<T>>
 */
function open(array $event_names, int $pid = 0, int $cpu = -1): Handle
{
}

/**
 * Returns a borrowed view of the request handle, initialized on the worker's first request.
 * Closing the returned object detaches only that view.
 *
 * Initialization is retried on later requests if opening fails. Pending initialization or
 * lifecycle errors are thrown once when this function is called; subsequent calls return
 * null if the handle is unavailable for this request.
 * An unconsumed error remains pending even if a later request successfully opens the handle.
 * @throws PmuEventNotFoundException|IOException if the handle could not be prepared
 * @phpstan-return ?Handle<list<string>>
 */
function request_handle(): ?Handle
{
}

/**
 * Handles returned by open() own their native descriptors and release them on close() or destruction.
 * Handles returned by request_handle() are borrowed views of persistent native state.
 *
 * @phpstan-template T of list<string>
 */
final class Handle
{
    /**
     * Releases owned descriptors immediately or detaches this borrowed view.
     * This method is idempotent; every other method throws ClosedException after it is called.
     *
     * @throws IOException
     */
    final public function close(): void
    {
    }

    /**
     * @return $this
     * @throws ClosedException|IOException
     */
    final public function enable(): self
    {
    }

    /**
     * @return $this
     * @throws ClosedException|IOException
     */
    final public function disable(): self
    {
    }

    /**
     * Get a raw byte stream backed by a duplicate of one of the handle's file descriptors.
     *
     * Index 0 is the event-group leader; requested events begin at index 1 in request order.
     * @note the returned stream owns an independent copy of the file descriptor, so closing it
     *       does not affect this handle or subsequent calls to read()
     * @return resource
     * @throws ClosedException|IOException
     * @throws \ValueError if idx does not reference an existing file descriptor
     */
    final public function rawStream(int $idx = 0)
    {
    }

    /**
     * Returns raw counts since opening or the latest reset, with kernel-lifetime timing totals.
     *
     * @throws ClosedException|OverflowException|IOException
     *
     * @phpstan-return ReadResult<T>
     */
    final public function read(): ReadResult
    {
    }

    /**
     * @throws ClosedException|OverflowException|IOException
     *
     * @phpstan-return array<value-of<T>, int>
     */
    final public function readArray(): array
    {
    }

    /**
     * Clears counts while preserving the enabled state. Lifetime timing totals are not cleared.
     * Active counters are briefly disabled to capture the timing baseline used by phpinfo().
     *
     * @return $this
     * @throws ClosedException|IOException
     */
    final public function reset(): self
    {
    }
}

/**
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
final class PmuInfo
{
    /**
     * Symbolic PMU name, usable as an event-string prefix.
     */
    public readonly string $name;
    public readonly string $desc;
    /**
     * Unique PMU identifier, matching the requested $pmu in get_pmu_info().
     */
    public readonly int $pmu;
    public readonly int $type;
    /**
     * Number of available events for this PMU model on the host; valid only when $is_present is true.
     */
    public readonly int $nevents;
    /**
     * Whether this PMU model was detected on the host.
     */
    public readonly bool $is_present;
}

/**
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_event_info.html
 */
final class PmuEventInfo
{
    public readonly string $name;
    public readonly string $desc;
    /**
     * Complete equivalent event string for shortcut events, when provided; otherwise null.
     */
    public readonly ?string $equiv;
    /**
     * Identifier of the PMU model that owns this event.
     */
    public readonly int $pmu;
    /**
     * Libpfm event index for get_pmu_event_info().
     */
    public readonly int $idx;
    /**
     * Whether this PMU model was detected on the host.
     */
    public readonly bool $is_present;
}
