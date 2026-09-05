--TEST--
Request counter initialization errors are deferred and consumed once
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--INI--
perfidious.request.enable=1
perfidious.request.metrics=blahblahblah
--FILE--
<?php
echo "request body started\n";
try {
    Perfidious\request_handle();
} catch (Perfidious\PmuEventNotFoundException $error) {
    echo $error->getMessage(), "\n";
}
var_dump(Perfidious\request_handle());
echo "request body completed\n";
?>
--EXPECT--
request body started
failed to get libpfm event encoding for blahblahblah: event not found
NULL
request body completed
