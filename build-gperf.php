<?php

foreach (PerfExt\list_pmus() as $pmu) {
    echo $pmu['name'], ',', $pmu['pmu'], PHP_EOL;
}
