--TEST--
PMU metadata lookups reject invalid identifiers without narrowing PHP integers
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--FILE--
<?php

function expectIdentifierError(string $label, int $id, string $class, callable $lookup, int $code = -2): void
{
    try {
        $lookup($id);
        echo "$label accepted $id\n";
    } catch (Throwable $error) {
        if (get_class($error) !== $class || $error->getCode() !== $code) {
            printf("%s(%d): unexpected %s, code %d\n", $label, $id, get_class($error), $error->getCode());
        }
        if (!str_contains($error->getMessage(), "for $id:")) {
            printf("%s(%d): diagnostic changed the identifier: %s\n", $label, $id, $error->getMessage());
        }
    }
}

$pmu = Perfidious\get_pmu_info(51);
$event = Perfidious\list_pmu_events($pmu->pmu)[0];
$lookups = [
    'get_pmu_info' => static fn(int $id) => Perfidious\get_pmu_info($id),
    'get_pmu_event_info PMU' => static fn(int $id) => Perfidious\get_pmu_event_info($id, $event->idx),
    'list_pmu_events' => static fn(int $id) => Perfidious\list_pmu_events($id),
];

$invalidPmus = [-1, PHP_INT_MIN, PHP_INT_MAX];
$invalidEvents = [-1, PHP_INT_MIN, PHP_INT_MAX, 12345];
if (PHP_INT_SIZE >= 8) {
    // Both signs can lose high bits and turn into a valid native identifier.
    foreach ([-4294967296, 4294967296] as $offset) {
        $invalidPmus[] = $pmu->pmu + $offset;
        $invalidEvents[] = $event->idx + $offset;
    }
    $invalidPmus[] = 2147483648;
    $invalidEvents[] = 2147483648;
    $invalidEvents[] = -2147483649;
}

foreach ($lookups as $label => $lookup) {
    // PFM_PMU_NONE keeps its existing unsupported-PMU error.
    expectIdentifierError($label, 0, Perfidious\PmuNotFoundException::class, $lookup, -1);
    foreach ($invalidPmus as $id) {
        expectIdentifierError($label, $id, Perfidious\PmuNotFoundException::class, $lookup);
    }
}
foreach ($invalidEvents as $idx) {
    expectIdentifierError(
        'get_pmu_event_info event', $idx, Perfidious\PmuEventNotFoundException::class,
        static fn(int $id) => Perfidious\get_pmu_event_info($pmu->pmu, $id),
    );
}

// PMU lookup keeps precedence when both identifiers are invalid.
expectIdentifierError(
    'get_pmu_event_info invalid PMU precedence', PHP_INT_MAX, Perfidious\PmuNotFoundException::class,
    static fn(int $id) => Perfidious\get_pmu_event_info($id, PHP_INT_MAX),
);
expectIdentifierError(
    'get_pmu_event_info unsupported PMU precedence', 0, Perfidious\PmuNotFoundException::class,
    static fn(int $id) => Perfidious\get_pmu_event_info($id, -1), -1,
);
echo "invalid identifiers rejected with their original values\n";

// Rejected lookups must not disturb enumeration or subsequent valid metadata.
var_dump(Perfidious\get_pmu_info($pmu->pmu) == $pmu);
var_dump(Perfidious\get_pmu_event_info($pmu->pmu, $event->idx) == $event);
var_dump(Perfidious\list_pmu_events($pmu->pmu)[0] == $event);
var_dump(in_array($pmu, Perfidious\list_pmus()));
?>
--EXPECT--
invalid identifiers rejected with their original values
bool(true)
bool(true)
bool(true)
bool(true)
