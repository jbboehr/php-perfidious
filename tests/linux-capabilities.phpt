--TEST--
Linux effective capabilities are parsed without integer-width assumptions
--FILE--
<?php
require __DIR__ . '/linux-capabilities.inc';

$statuses = [
    "Name:\tphp\nCapEff:\t0000004000000000\n" => true,
    "CapEff:\t0000000000000000\n" => false,
    "CapEff:\t0000002000000000\n" => false,
    "CapEff:\t0000008000000000\n" => false,
    "CapEff:\t0000004000000000   \r\n" => true,
    "Name:\tphp\n" => null,
];

foreach ($statuses as $status => $expected) {
    var_dump(perfidious_linux_status_has_effective_capability($status, 38) === $expected);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
