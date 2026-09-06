# Sampler API

This document describes the sampler implemented in the current source tree. See the [support matrix](#support-matrix)
for available metric/scope combinations and [lifecycle and errors](#lifecycle-and-errors) for validation and failure
behavior. Unimplemented extensions are described under [future work](#future-work).

## Summary

The sampler API provides one vocabulary and lifecycle for a small set of counters while keeping the existing
`Perfidious`, `Perfidious\Windows`, and `Perfidious\Darwin` low-level APIs available. Unsupported metric/scope
combinations are rejected when opening the sampler.

The API has these properties:

- the current process is the default scope, while current-thread support is an opt-in platform capability;
- the caller requests a non-empty list of metrics explicitly;
- unsupported scope and metric combinations fail when the sampler is opened;
- a sampler begins counting when it is opened and returns cumulative values relative to that point;
- two samples from the same sampler can produce a delta;
- samples expose values through `value(Metric)` rather than a public keyed collection;
- metrics expose their units for generic reporting;
- missing counters are never represented by `null` or by a synthetic zero; and
- platform namespaces remain the place for richer platform-specific primitives.

There is no default metric list. A default containing every metric would fail on common Windows and macOS
configurations, while a lowest-common-denominator default would be too limited to be useful.

There is a default scope: the current process. This matches typical PHP-FPM and CLI execution, where one process handles
one PHP request or program at a time. Thread scope remains opt-in for threaded runtimes and counters that exist only for
the current thread.

Metrics are string-backed so configuration adapters can use one canonical `Metric::from()` mapping. The backing values
are semantic identifiers such as `cpu-time`; they are not exposed as sample array keys and do not encode presentation
units. `Metric::unit()` exposes the measurement unit separately.

## API shape

These illustrative declarations summarize the current PHP interface. The extension provides the method implementations.
The [common stub](../stubs/common.stub.php) contains the complete declarations.

```php
namespace Perfidious;

enum Scope: string
{
    case CurrentProcess = 'current-process';
    case CurrentThread = 'current-thread';
}

enum MetricUnit
{
    case Nanoseconds;
    case Count;
}

enum Metric: string
{
    case CpuTime = 'cpu-time';
    case PageFaults = 'page-faults';
    case ContextSwitches = 'context-switches';
    case CpuCycles = 'cpu-cycles';
    case Instructions = 'instructions';

    public function unit(): MetricUnit
    {
    }
}

final class Sampler
{
    private function __construct()
    {
    }

    /** @param non-empty-list<Metric> $metrics */
    public static function open(array $metrics, Scope $scope = Scope::CurrentProcess): self
    {
    }

    /** @return non-empty-list<Metric> */
    public function metrics(): array
    {
    }

    public function read(): Sample
    {
    }

    public function close(): void
    {
    }
}

final class Sample
{
    private function __construct()
    {
    }

    public function value(Metric $metric): int
    {
    }

    public function since(Sample $earlier): SampleDelta
    {
    }
}

final class SampleDelta
{
    private function __construct()
    {
    }

    /** Monotonic time between the completion of the two reads. */
    public readonly int $elapsedTimeNs;

    public function value(Metric $metric): int
    {
    }
}

final class UnsupportedMetricException extends \RuntimeException implements ExceptionInterface
{
    private function __construct()
    {
    }

    public readonly Scope $scope;

    /** @var non-empty-list<Metric> */
    public readonly array $unsupportedMetrics;
}

final class ClosedException extends \LogicException implements ExceptionInterface
{
}

final class WrongThreadException extends \LogicException implements ExceptionInterface
{
}

final class ResourceBusyException extends \RuntimeException implements ExceptionInterface
{
}
```

Typical use:

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

Cross-platform applications that want to degrade gracefully should catch `UnsupportedMetricException`, remove its
`$unsupportedMetrics` from the requested set, and retry. The exception's `$scope` identifies the rejected request;
its message remains a human-readable summary rather than the machine-readable recovery interface. The exception is
created only by the extension and is not serializable, ensuring that its readonly metadata is always initialized.

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

Windows supports `Metric::CpuTime`, `Metric::ContextSwitches`, and `Metric::CpuCycles` for this scope. Windows
current-thread page faults and instructions still throw `UnsupportedMetricException`. Darwin supports
`Metric::CpuTime`; its other current-thread metrics remain unsupported.

Linux intentionally rejects all `Scope::CurrentThread` requests. `perf_event_open()` can bind a
counter group to the calling native thread without enumerating the process's threads, but PHP-FPM and CLI normally
execute PHP on one native thread. For those callers, thread scope usually adds no useful isolation over process scope.
Revisit this decision when a ZTS or embedded-PHP consumer needs to exclude work performed by other native threads.

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

Linux current-thread support is deferred until it has a concrete PHP use case. It is not available through the current
sampler API.

The important consequences are:

- no five-counter preset works across every platform and scope;
- Linux current-thread support is deliberately deferred despite kernel support;
- Linux process-wide cycles and instructions must not be implemented by attaching `perf_event_open()` only to the
  calling thread;
- Windows process context switches and instructions have no honest mapping in the current backend;
- Windows thread page faults have no honest mapping in the current backend;
- Darwin thread page faults and context switches have no honest mapping in the current backend; and
- Darwin cycle and instruction fields can remain zero on hardware or virtual machines where the kernel cannot collect
  them, so the high-level sampler probes process cycles when opening and leaves instructions in the low-level API.

Windows thread instructions are not supported by the sampler. `EnableThreadProfiling()`
can expose globally configured hardware counters, but configuring those counters requires a kernel driver and the
current low-level API cannot prove that a selected index represents retired instructions. Applications that control
such a driver can continue to use `Perfidious\Windows\enable_current_thread_profiling()` directly.

## Backend mapping

### Linux

The [Linux sampler backend](../src/linux/sampler.c) uses `getrusage(RUSAGE_SELF)` for process-wide CPU time, page
faults, and context switches. CPU time combines `ru_utime` and `ru_stime`, page faults combine `ru_minflt` and
`ru_majflt`, and context switches combine `ru_nvcsw` and `ru_nivcsw`.

The common sampler does not use the low-level `Perfidious\open()` perf-event backend. Process cycles, instructions,
and all current-thread requests are unsupported. A possible current-thread perf-event backend is described under
[future work](#future-work).

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
retry. These state errors are deliberately not subclasses of `IOException`.

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

- Linux current-thread support remains deferred until a ZTS or embedded-PHP consumer demonstrates a need for it.
- Additional Darwin current-thread metrics require a usable native source and reliable availability checks.
- Instruction counting remains proposed. Its intended metric is the native retired-instruction count charged to the
  selected scope. Interrupt and speculative execution accounting can vary by processor and operating system, so it
  would support deltas on one host without promising cross-machine comparability. Darwin instruction fields and
  driver-dependent Windows hardware counters remain in their low-level namespaces.

Future additions should use the same classes on every platform, test successful combinations, and test that unsupported
combinations fail without leaking partially opened native resources.

### Deferred Linux current-thread backend

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
