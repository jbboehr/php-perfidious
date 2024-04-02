<?php

namespace PerfExt;

const VERSION = "0.1.0";


/**
 * @throws PmuNotFoundException
 * @see https://perfmon2.sourceforge.net/manv4/pfm_get_pmu_info.html
 */
function get_pmu_info(int $pmu): PmuInfo
{
}

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
 */
function open(array $event_names, int $pid = 0, int $cpu = -1): Handle
{
}

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

final class Handle
{
    /**
     * @throws IOException
     */
    public function enable(): self {}

    /**
     * @throws IOException
     */
    public function disable(): self {}

    /**
     * @return array<string, int>
     * @throws OverflowException|IOException
     */
    public function read(): array {}

    /**
     * @throws IOException
     */
    public function reset(): self {}
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
