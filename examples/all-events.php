#!/usr/bin/env php
<?php

use function Perfidious\list_pmus;
use function Perfidious\list_pmu_events;

$opts = getopt('', [
    'all',
    'pmu:',
    'help',
], $rest_index);

$pmu = $opts['pmu'] ?? null;

if (array_key_exists('help', $opts)) {
    fprintf(STDERR, "Usage: " . $argv[0] . PHP_EOL);
    fprintf(STDERR, PHP_EOL);
    fprintf(STDERR, "Show event names supported by libpfm4" . PHP_EOL);
    fprintf(STDERR, PHP_EOL);
    fprintf(STDERR, "    --all      Show all PMUs, not just active" . PHP_EOL);
    fprintf(STDERR, "    --pmu      Search for a specific PMU" . PHP_EOL);
    exit(0);
}

foreach (list_pmus() as $pmu_info) {
    $matches_pmu = (
        array_key_exists('all', $opts) ||
        !($pmu !== null && $pmu !== $pmu_info->pmu && !str_contains($pmu_info->name, $pmu))
    );
    if (!$matches_pmu) {
        continue;
    }
    foreach (list_pmu_events($pmu_info->pmu) as $pmu_event_info) {
        if ($pmu_event_info->is_present) {
            echo $pmu_event_info->name, ' : supported', PHP_EOL;
        } else {
            echo $pmu_event_info->name, ' : unavailable', PHP_EOL;
        }
    }
}
