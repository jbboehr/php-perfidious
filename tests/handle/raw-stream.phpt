--TEST--
Perfidious\Handle::rawStream()
--EXTENSIONS--
perfidious
--SKIPIF--
<?php require __DIR__ . '/../skipif-linux-only.inc'; ?>
--FILE--
<?php
$method = new ReflectionMethod(Perfidious\Handle::class, 'rawStream');
$parameters = $method->getParameters();
$idx = $parameters[0];

var_dump(!$method->hasReturnType());
var_dump($method->getNumberOfRequiredParameters() === 0);
var_dump(count($parameters) === 1);
var_dump($idx->getName() === 'idx');
var_dump((string) $idx->getType() === 'int');
var_dump(!$idx->allowsNull());
var_dump($idx->isOptional());
var_dump($idx->isDefaultValueAvailable());
var_dump($idx->getDefaultValue() === 0);

(function () {
    $handle = Perfidious\open([
        "perf::PERF_COUNT_SW_CPU_CLOCK:u",
    ]);
    $handle->enable();
    $stream = $handle->rawStream();
    var_dump(strlen(fread($stream, 32)));
    var_dump($handle->read());
})();
gc_collect_cycles();

(function () {
    $handle = Perfidious\open([
        "perf::PERF_COUNT_SW_CPU_CLOCK:u",
        "perf::PERF_COUNT_SW_PAGE_FAULTS:u",
        "perf::PERF_COUNT_SW_CONTEXT_SWITCHES:u",
    ]);
    $handle->enable();

    foreach ([0, 1, 2, 3] as $idx) {
        $stream = $handle->rawStream($idx);
        printf("index %d: %s, %d bytes\n", $idx, get_resource_type($stream), strlen(fread($stream, 32)));
        fclose($stream);
    }

    try {
        $handle->rawStream(null);
        echo "accepted null\n";
    } catch (TypeError) {
        echo "rejected null\n";
    }

    $closed = $handle->rawStream(3);
    $survivor = $handle->rawStream(3);
    fclose($closed);
    printf("after sibling close: %d bytes\n", strlen(fread($survivor, 32)));

    unset($handle);
    gc_collect_cycles();
    printf("after handle destruction: %d bytes\n", strlen(fread($survivor, 32)));
    fclose($survivor);
})();
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(32)
object(Perfidious\ReadResult)#%d (3) {
  ["timeEnabled"]=>
  int(%d)
  ["timeRunning"]=>
  int(%d)
  ["values"]=>
  array(1) {
    ["perf::PERF_COUNT_SW_CPU_CLOCK:u"]=>
    int(%d)
  }
}
index 0: stream, 32 bytes
index 1: stream, 32 bytes
index 2: stream, 32 bytes
index 3: stream, 32 bytes
rejected null
after sibling close: 32 bytes
after handle destruction: 32 bytes
