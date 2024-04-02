<?php

namespace PerfExt;

const VERSION = "0.1.0";


/**
 * @throws PmuNotFoundException
 */
function get_pmu_info(int $pmu): PmuInfo
{
}

function global_handle(): ?Handle
{
}

/**
 * @return list<PmuInfo>
 */
function list_pmus(): array
{
}

/**
 * @return list<PmuEventInfo>
 * @throws PmuNotFoundException
 */
function list_pmu_events(int $pmu): array
{
}

function open(array $event_names): Handle
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

final class PmuInfo
{
    public string $name;
    public string $desc;
    public int $pmu;
    public int $type;
    public int $nevents;
}

final class PmuEventInfo
{
    public string $name;
    public string $desc;
    public ?string $equiv;
    public int $pmu;
    public bool $is_present;
}
