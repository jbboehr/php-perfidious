--TEST--
invalid configured overflow mode fails safe to throw
--EXTENSIONS--
perfidious
--SKIPIF--
<?php if (!Perfidious\DEBUG) die("skip: must be compiled in debug mode"); ?>
--INI--
perfidious.overflow_mode=2oops
--FILE--
<?php
try {
    Perfidious\debug_uint64_overflow();
    echo "debug conversion unexpectedly succeeded\n";
} catch (Perfidious\OverflowException) {
    echo "debug conversion rejected invalid mode\n";
}

$handle = Perfidious\open([
    'perf::PERF_COUNT_SW_CPU_CLOCK:u',
]);

foreach (['read', 'readArray'] as $method) {
    $handle->debugInjectOverflowRead();

    try {
        $handle->$method();
        echo "$method unexpectedly succeeded\n";
    } catch (Perfidious\OverflowException) {
        echo "$method rejected invalid mode\n";
    }
}
--EXPECT--
debug conversion rejected invalid mode
read rejected invalid mode
readArray rejected invalid mode
