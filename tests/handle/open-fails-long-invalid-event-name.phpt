--TEST--
Perfidious\Handle::open() - invalid event name long enough to overflow the internal message buffer
--EXTENSIONS--
perfidious
--FILE--
<?php
$prefix = "failed to get libpfm event encoding for ";
$event_name = str_repeat("X", 600);

try {
    Perfidious\open([$event_name]);
    echo "FAIL: no exception thrown\n";
} catch (Perfidious\PmuEventNotFoundException $e) {
    $msg = $e->getMessage();
    $expected = substr($prefix . $event_name, 0, 511);

    var_dump(strlen($msg) === 511);
    var_dump($msg === $expected);
}
--EXPECT--
bool(true)
bool(true)
