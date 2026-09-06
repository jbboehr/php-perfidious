# Sampler API

The sampler measures an explicit list of metrics for the current process or native thread. Opening starts cumulative
measurement; two samples from the same sampler produce a delta. Missing counters raise an exception instead of
returning `null` or a synthetic zero. The low-level platform APIs remain available for richer native data.

See the [support matrix](#support-matrix) for implemented combinations, [lifecycle and errors](#lifecycle-and-errors)
for failure behavior, and [future work](#future-work) for proposed extensions.

## Usage

```php
use Perfidious\Metric;
use Perfidious\Sampler;

$sampler = Sampler::open([
    Metric::CpuTime,
    Metric::PageFaults,
]);

try {
    $before = $sampler->read();

    hash('sha256', str_repeat('x', 1_000_000));

    $delta = $sampler->read()->since($before);
    printf("CPU time: %d ns\n", $delta->value(Metric::CpuTime));
    printf("Page faults: %d\n", $delta->value(Metric::PageFaults));
} finally {
    $sampler->close();
}
```

On Windows and macOS, current-thread CPU time is requested explicitly with the same metrics-first call shape:

```php
use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\Scope;

$sampler = Sampler::open([Metric::CpuTime], Scope::CurrentThread);
$sampler->close();
```

## API shape

All names below are in `Perfidious`. The [common stub](../stubs/common.stub.php) contains the complete declarations.
`Sampler`, `Sample`, and `SampleDelta` are final classes with private constructors.

| Operation | Signature or property |
| --- | --- |
| Open a sampler | `Sampler::open(array $metrics, Scope $scope = Scope::CurrentProcess): self` |
| List configured metrics in request order | `Sampler::metrics(): array` returns `non-empty-list<Metric>` |
| Read cumulative counters | `Sampler::read(): Sample` |
| Release native resources | `Sampler::close(): void` |
| Read a sampled value | `Sample::value(Metric $metric): int` |
| Subtract an earlier sample | `Sample::since(Sample $earlier): SampleDelta` |
| Read a counter difference | `SampleDelta::value(Metric $metric): int` |
| Read the monotonic time between completed reads | `SampleDelta::$elapsedTimeNs` is a readonly `int` |
| Determine a metric's unit | `Metric::unit(): MetricUnit` |

`$metrics` must be a non-empty list of unique `Metric` cases. There is no default metric list: selecting every metric
would fail on common hosts, while a preset limited to the shared metrics would be too restrictive. The default scope
is `Scope::CurrentProcess`.

`Metric` and `Scope` are string-backed enums for configuration through `Metric::from()` and `Scope::from()`:

| Enum case | Backing value |
| --- | --- |
| `Metric::CpuTime` | `cpu-time` |
| `Metric::PageFaults` | `page-faults` |
| `Metric::ContextSwitches` | `context-switches` |
| `Metric::CpuCycles` | `cpu-cycles` |
| `Metric::Instructions` | `instructions` |
| `Scope::CurrentProcess` | `current-process` |
| `Scope::CurrentThread` | `current-thread` |

Backing values identify metrics; samples expose values through `value(Metric)`, without public array keys.
`MetricUnit` has the cases `Nanoseconds` and `Count`; units are separate from metric identifiers.

Cross-platform applications that want to degrade gracefully should catch `UnsupportedMetricException`, remove its
`$unsupportedMetrics` from the requested set, and retry. That readonly property is a `non-empty-list<Metric>`; the
readonly `Scope $scope` identifies the rejected request. The message is a human-readable summary. The exception has a
private constructor, is created only by the extension, and is not serializable, so its metadata is always initialized.

An advisory capability-discovery API is deliberately omitted. Hardware availability and
permissions can change between a capability check and `Sampler::open()`, so opening the sampler must remain the
authoritative check. Capability discovery can be added later if real applications demonstrate a need for it.

## Scope

`Scope::CurrentProcess` means the current operating-system process, including all of its threads and excluding child
processes. It is the default because it matches the execution model used by typical PHP-FPM workers and CLI programs.
It must not silently degrade to the thread that happens to call `Sampler::open()`.

`Scope::CurrentThread` means the native operating-system thread that calls `Sampler::open()`. The sampler must be read
and closed from that same thread. It is an advanced scope for ZTS builds, threaded or embedded SAPIs, and native
counters that are unavailable process-wide. PHP fibers share an operating-system thread, so thread scope does not
isolate one fiber from another.

See the [support matrix](#support-matrix) for current-thread availability and the
[deferred Linux backend](#deferred-linux-current-thread-backend) for its rationale and proposed mapping.

The sampler does not target arbitrary process or thread identifiers. The platform-specific APIs can continue to
expose facilities that do so.

## Metric semantics

All values are unsigned cumulative counters internally and non-negative PHP integers publicly. Samples are relative to
the point at which the sampler was successfully opened, rather than process or thread creation. This hides the different
native baselines used by Linux, Windows, and macOS.

### CPU time

`Metric::CpuTime` is user plus kernel CPU time charged to the selected scope, expressed in nanoseconds. It is CPU time,
not elapsed wall-clock time. On a multithreaded process, process CPU time can increase faster than wall time.

This deliberately combines user and kernel time. The low-level APIs remain available when an application needs the two
components separately.

### Page faults

`Metric::PageFaults` is the total number of minor and major page faults charged to the selected scope. The sampler API
does not expose the split because Windows' public process counter supplies only the total.

### Context switches

`Metric::ContextSwitches` is the total number of voluntary and involuntary context switches charged to the selected
scope. The sampler API does not expose the split because Windows thread profiling supplies only the total.

### CPU cycles

`Metric::CpuCycles` is the native cycle count charged to the selected scope. It includes user and kernel execution when
the native interface can provide both. Cycle counts are affected by processor frequency, architecture, virtualization,
and native accounting rules, so deltas are useful on one host but should not be compared across machines.

### Instructions

`Metric::Instructions` names the retired-instruction count, but every current sampler backend rejects it with
`UnsupportedMetricException`. The enum case reserves the metric name. Its proposed semantics are described under
[future work](#future-work).

## Support matrix

This matrix describes the implemented sampler backends. `Yes` means the backend supports the combination, subject to
native permissions, resource availability, and call failures. `No` means `Sampler::open()` rejects the combination
with `UnsupportedMetricException`.

| Metric | Linux process | Linux thread | Windows process | Windows thread | Darwin process | Darwin thread |
| --- | --- | --- | --- | --- | --- | --- |
| CPU time | Yes | No | Yes | Yes | Yes | Yes |
| Page faults | Yes | No | Yes | No | Yes | No |
| Context switches | Yes | No | No | Yes | Yes | No |
| CPU cycles | No | No | Yes | Yes | Probed | No |
| Instructions | No | No | No | No | No | No |

`Probed` means that `Sampler::open()` accepts the metric only when the host reports a positive cumulative native count;
a zero count is treated as unavailable. CPU time and page faults are the shared metric set supported by all three
process backends.

## Backend mapping

### Linux

The [Linux sampler backend](../src/linux/sampler.c) uses `getrusage(RUSAGE_SELF)` for process-wide CPU time, page
faults, and context switches. CPU time combines `ru_utime` and `ru_stime`, page faults combine `ru_minflt` and
`ru_majflt`, and context switches combine `ru_nvcsw` and `ru_nivcsw`.

The common sampler does not use the low-level `Perfidious\open()` perf-event backend. A possible perf-event mapping
and the requirement to account for all process threads are described under [future work](#future-work).

### Windows

Process CPU time maps to
[`GetProcessTimes()`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes),
page faults to
[`PROCESS_MEMORY_COUNTERS_EX::PageFaultCount`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex),
and cycles to
[`QueryProcessCycleTime()`](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryprocesscycletime).
These facilities all aggregate the process or its threads as required by `Scope::CurrentProcess`.

Thread CPU time maps to the public `Perfidious\Windows\get_current_thread_times()` wrapper around `GetThreadTimes()`.
Thread context switches and cycles map to
[`EnableThreadProfiling()`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-enablethreadprofiling)
and
[`PERFORMANCE_DATA`](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-performance_data).

Windows thread instructions are not supported by the sampler. `EnableThreadProfiling()`
can expose globally configured hardware counters, but configuring those counters requires a kernel driver and the
current low-level API cannot prove that a selected index represents retired instructions. Applications that control
such a driver can continue to use `Perfidious\Windows\enable_current_thread_profiling()` directly.

### Darwin

The existing process snapshot already combines `proc_pid_rusage(RUSAGE_INFO_V4)` with `getrusage(RUSAGE_SELF)`. XNU's
resource-accounting documentation describes task time, cycles, and instructions, while `getrusage()` supplies minor and
major page faults plus voluntary and involuntary context switches. See XNU's
[`recount` documentation](https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/recount.md) and
[`getrusage(2)` manual](https://github.com/apple/darwin-xnu/blob/main/bsd/man/man2/getrusage.2).

The existing thread snapshot supplies CPU time and, where supported, cycles and instructions through
`thread_selfcounts()`, with `THREAD_BASIC_INFO` as the time-only fallback. It does not supply thread page faults or
context switches.

The low-level Darwin API preserves the warning that a zero cycle or instruction count can mean either no observed
events or unavailable kernel accounting. A process has already consumed CPU cycles by the time it opens a sampler, so
the sampler treats a positive cumulative cycle count as evidence that accounting is available and then establishes its
native baseline. A zero count causes `Sampler::open()` to reject `Metric::CpuCycles`. Retired instructions remain
available only through the low-level snapshot API.

## Lifecycle and errors

`Sampler::open()` validates the complete request before returning. An empty metric list or duplicate metrics throw
`ValueError`. A list element that is not a `Metric`, including a string such as `'cpu-time'`, throws `TypeError`.
If any metric is unsupported for the selected scope, opening throws
`UnsupportedMetricException` and releases every native resource acquired while evaluating the request.
Known-unsupported metrics are rejected before fallible host-capability probes; a probe is performed only when every
requested metric is nominally supported for the selected platform and scope.

Reading an explicitly closed handle, sampler, or thread profile throws `ClosedException`. Reading a Windows or Darwin
current-thread sampler from a different native thread throws `WrongThreadException`. A conflicting Windows thread
profiling session throws `ResourceBusyException`; callers can release the existing sampler or low-level profile and
retry. `ClosedException` and `WrongThreadException` extend `LogicException`; `ResourceBusyException`,
`UnsupportedMetricException`, and `IOException` extend `RuntimeException`. All are final and implement
`Perfidious\ExceptionInterface`. The state errors are not subclasses of `IOException`.

Native permission, resource, and call failures use `IOException`. Counter values that do not
fit in a PHP integer use `OverflowException`.

An open sampler begins counting immediately. `close()` is idempotent, and destruction closes an unclosed sampler.
`Sampler::metrics()` remains available after closing. Samplers, samples, and deltas are not cloneable or serializable.

A failed `read()` produces no partial sample. Native counters continue running, and the sampler remains usable unless
the underlying facility is irrecoverably closed or invalidated.

`Sample::value()` and `SampleDelta::value()` throw `ValueError` when asked for a metric that was not configured.
`Sampler::metrics()` returns the configured enum cases in request order, allowing generic consumers to iterate without
exposing the internal values collection or its storage keys. `Metric::unit()` returns `MetricUnit::Nanoseconds` for
CPU time and `MetricUnit::Count` for page faults, context switches, CPU cycles, and instructions.

`Sample::since()` accepts a sample from the same sampler that is not newer than the receiver. Passing a sample from
another sampler or a later sample throws `ValueError`; subtracting a sample from itself returns a zero delta. The
resulting values are unsigned counter differences, and `elapsedTimeNs` uses a monotonic clock measured at the completion
of each native read.

Backends must account for the width of each native counter. In particular, Windows process page faults and thread
context switches, and Linux process context switches on 32-bit targets, are 32-bit counters. The sampler widens each
counter independently across successive reads. More than one wrap between reads is not observable and remains a native
limitation.

Native sources are not guaranteed to produce one atomic cross-metric snapshot. A backend may call several operating
system interfaces sequentially, so a sample is a closely grouped observation rather than a single instant. Samples do
not expose timestamp bounds for individual metric reads.

## Non-goals

The sampler API does not:

- replace or deprecate any low-level API;
- make counter values directly comparable across operating systems or CPU models;
- target arbitrary processes, threads, CPUs, or cgroups;
- enumerate arbitrary PMU events;
- hide missing platform capabilities with `null`, zero, or a different scope;
- aggregate child processes; or
- promise an atomic snapshot across multiple native calls.

## Future work

The shared API and the `Yes`/`Probed` combinations in the support matrix are implemented. The following extensions are
proposals, not currently available sampler behavior:

- Additional Darwin current-thread metrics require a usable native source and reliable availability checks.
- Instruction counting remains proposed. Its intended metric is the native retired-instruction count charged to the
  selected scope. Interrupt and speculative execution accounting can vary by processor and operating system, so it
  would support deltas on one host without promising cross-machine comparability. Darwin instruction fields and
  driver-dependent Windows hardware counters remain in their low-level namespaces.

Future additions should use the same classes on every platform, test successful combinations, and test that unsupported
combinations fail without leaking partially opened native resources.

### Deferred Linux current-thread backend

Linux thread support is deferred until a ZTS or embedded-PHP consumer needs it. Typical PHP-FPM and CLI programs execute
PHP on one native thread, so thread scope usually adds no useful isolation for those callers. A consumer that needs to
exclude work by other native threads would justify revisiting this decision.

Current-thread metrics could map to a `perf_event_open()` group containing the requested perf events. The Linux API
defines `pid == 0` and `cpu == -1` as the calling thread. See the
[Linux `perf_event_open(2)` documentation](https://www.kernel.org/pub/linux/docs/man-pages/book/man-pages-6.17.pdf).

| Sampler metric | Proposed Linux event |
| --- | --- |
| `Metric::CpuTime` | `PERF_COUNT_SW_CPU_CLOCK` |
| `Metric::PageFaults` | `PERF_COUNT_SW_PAGE_FAULTS` |
| `Metric::ContextSwitches` | `PERF_COUNT_SW_CONTEXT_SWITCHES` |
| `Metric::CpuCycles` | `PERF_COUNT_HW_CPU_CYCLES` |
| `Metric::Instructions` | `PERF_COUNT_HW_INSTRUCTIONS` |

The existing `Perfidious\open()` path always excludes kernel and hypervisor events. A future sampler perf backend
would need separate configuration because its CPU time, cycle, and instruction definitions include kernel execution.
It would also need to scale multiplexed hardware counters using the kernel's enabled and running times and document
that the result is an estimate. Counter-quality metadata could be added if applications need to distinguish scaled
readings.

Process-wide cycles and instructions require a correct all-thread implementation. Linux identifies perf targets by
task/thread, and targeting the process ID counts only the thread-group leader. The `inherit` flag omits existing
threads and is incompatible with some grouped read formats. Process thread tracking remains deferred until there is
a demonstrated consumer.
