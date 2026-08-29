--TEST--
The shipped stub matches the runtime public API contract
--EXTENSIONS--
perfidious
--FILE--
<?php
require __DIR__ . '/public-api-contract.inc';

$stubRoot = dirname(__DIR__) . '/stubs';
$stubFiles = [
    $stubRoot . '/common.stub.php',
    sprintf('%s/%s.stub.php', $stubRoot, strtolower(PHP_OS_FAMILY)),
];
$stubPhp = getenv('PERFIDIOUS_STUB_PHP') ?: PHP_BINARY;
$process = proc_open(
    [$stubPhp, '-n', __DIR__ . '/public-api-stub.inc', ...$stubFiles],
    [
        ['pipe', 'r'],
        ['pipe', 'w'],
        ['pipe', 'w'],
    ],
    $pipes,
);

if (!is_resource($process)) {
    throw new RuntimeException('Could not start the stub reflection process');
}

fclose($pipes[0]);
$stubJson = stream_get_contents($pipes[1]);
$stderr = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
$exitCode = proc_close($process);

if ($exitCode !== 0) {
    throw new RuntimeException(sprintf('Stub reflection failed with exit code %d: %s', $exitCode, $stderr));
}

$runtime = perfidious_contract_snapshot(true);
$stub = json_decode($stubJson, true, 512, JSON_THROW_ON_ERROR);
$differences = perfidious_contract_diff($runtime, $stub);

if ($differences === []) {
    echo "API contract matches\n";
} else {
    echo implode("\n", $differences), "\n";
}
--EXPECT--
API contract matches
