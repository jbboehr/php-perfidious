<?php

for ($i = 0; $i < 10; $i++) {
    try {
        $handle = Perfidious\open([
            "perf::PERF_COUNT_SW_CPU_CLOCK:u",
        ], pid: getmypid());
        $handle->close();
    } catch (Perfidious\IOException) {
        // The capability and perf-event checks may reject the request; either path must release libcap state.
    }
}
