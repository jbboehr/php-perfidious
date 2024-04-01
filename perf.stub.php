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

final class PmuNotFoundException extends \InvalidArgumentException
{
}

final class Handle
{
    public function enable(): self {}

    public function disable(): self {}

    /**
     * @return array<string, int>
     */
    public function read(): array {}

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
