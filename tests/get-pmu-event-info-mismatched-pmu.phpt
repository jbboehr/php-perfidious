--TEST--
PMU event lookup rejects events owned by another PMU
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/skipif-linux-only.inc';
$pmus = array_filter(Perfidious\list_pmus(), static fn($pmu) => $pmu->nevents > 0);
if (count($pmus) < 2) {
    die('skip: requires two PMUs with events in the libpfm database');
}
?>
--FILE--
<?php

$pmus = array_values(array_filter(Perfidious\list_pmus(), static fn($pmu) => $pmu->nevents > 0));
[$first, $second] = $pmus;
// Prefer different presence flags so both metadata fields are exercised when possible.
foreach ($pmus as $pmu) {
    if ($pmu->is_present !== $first->is_present) {
        $second = $pmu;
        break;
    }
}

foreach ([[$first, $second], [$second, $first]] as [$owner, $other]) {
    $events = Perfidious\list_pmu_events($owner->pmu);
    foreach ($events as $listedEvent) {
        if ($listedEvent->pmu !== $owner->pmu ||
            !str_starts_with($listedEvent->name, $owner->name . '::') ||
            $listedEvent->is_present !== $owner->is_present) {
            echo "event enumeration returned inconsistent PMU metadata\n";
            break;
        }
    }
    $event = $events[0];
    try {
        $result = Perfidious\get_pmu_event_info($other->pmu, $event->idx);
        printf("mismatched pair accepted: requested PMU %d, returned PMU %d\n", $other->pmu, $result->pmu);
    } catch (Perfidious\PmuEventNotFoundException $error) {
        // PFM_ERR_NOTFOUND: the event does not exist within the requested PMU.
        if ($error->getCode() !== -4) {
            echo "unexpected mismatch error code: ", $error->getCode(), "\n";
        }
        if (!str_contains($error->getMessage(), "event $event->idx") ||
            !str_contains($error->getMessage(), "pmu $other->pmu")) {
            echo "mismatch diagnostic omitted the requested identifiers\n";
        }
    }

    // Rejected pairs must not disturb correct lookups or enumeration.
    $result = Perfidious\get_pmu_event_info($owner->pmu, $event->idx);
    if ($result != $event || $result->pmu !== $owner->pmu ||
        !str_starts_with($result->name, $owner->name . '::') ||
        $result->is_present !== $owner->is_present) {
        echo "correct lookup returned inconsistent PMU metadata\n";
    }
    if (Perfidious\list_pmu_events($owner->pmu) != $events) {
        echo "mismatch lookup changed event enumeration\n";
    }
}
echo "mismatched pairs rejected and correct metadata preserved\n";
?>
--EXPECT--
mismatched pairs rejected and correct metadata preserved
