<?php

namespace Perfidious;

const VERSION = "0.1.0";

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

interface Exception
{
}

final class IOException extends \RuntimeException implements Exception
{
}

final class OverflowException extends \OverflowException implements Exception
{
}

final class PmuNotFoundException extends \InvalidArgumentException implements Exception
{
}

final class PmuEventNotFoundException extends \InvalidArgumentException implements Exception
{
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
     * Get a raw byte stream from the handle's file descriptor
     *
     * @note closing this resource will cause subsequent calls to read to fail
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
