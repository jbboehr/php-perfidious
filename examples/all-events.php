#!/usr/bin/env php
<?php

// ok, these aren't all examples per se
// this one lists all or available PMU events

use function Perfidious\list_pmus;
use function Perfidious\list_pmu_events;

$opts = getopt('', [
    'show:',
    'help',
], $rest_index);

$show = $opts['show'] ?? 'present';
$show = is_string($show) ? $show : 'present';

if (array_key_exists('help', $opts)) {
    fprintf(STDERR, "Usage: " . $argv[0] . PHP_EOL);
    fprintf(STDERR, PHP_EOL);
    fprintf(STDERR, "Show event names supported by libpfm4" . PHP_EOL);
    fprintf(STDERR, PHP_EOL);
    fprintf(STDERR, "    --show PMU       \"present\", \"all\", a PMU ID, or a PMU name substring" . PHP_EOL);
    exit(0);
}

foreach (list_pmus() as $pmu_info) {
    $matches_pmu = match ($show) {
        'all' => true,
        'present' => $pmu_info->is_present,
        default => $pmu_info->pmu === (int) $show || str_contains($pmu_info->name, (string) $show),
    };
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
