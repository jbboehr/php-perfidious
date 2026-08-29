<?php

if ($argc < 2) {
    fwrite(STDERR, "usage: public-api-stub.inc <stub> [<stub> ...]\n");
    exit(2);
}

require __DIR__ . '/public-api-contract.inc';
foreach (array_slice($argv, 1) as $stubFile) {
    require $stubFile;
}

echo json_encode(perfidious_contract_snapshot(false), JSON_THROW_ON_ERROR);
