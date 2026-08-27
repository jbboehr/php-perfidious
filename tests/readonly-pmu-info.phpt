--TEST--
Perfidious\PmuInfo (readonly / no dynamic properties)
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/skipif-linux-only.inc'; ?>
--FILE--
<?php
$pmu = Perfidious\get_pmu_info(51);
try {
    $pmu->name = "x";
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $pmu->foo = "x";
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
Cannot modify readonly property Perfidious\PmuInfo::$name
Cannot create dynamic property Perfidious\PmuInfo::$foo
