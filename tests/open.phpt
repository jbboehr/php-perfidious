--TEST--
Perfidious\open()
--EXTENSIONS--
perfidious
--FILE--
<?php
(function () {
    $rv = Perfidious\open([
        "perf::PERF_COUNT_SW_CPU_CLOCK",
        "perf::PERF_COUNT_SW_PAGE_FAULTS",
        "perf::PERF_COUNT_SW_CONTEXT_SWITCHES",
    ]);
    var_dump(get_class($rv));
    $rv->enable();
    $rv->reset();
    $rv->disable();
})();
--EXPECTF--
string(%d) "Perfidious\Handle"