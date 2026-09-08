# Changelog

All Notable changes to `php-perfidious` will be documented in this file.

Updates should follow the [Keep a CHANGELOG](http://keepachangelog.com/) principles.

## [Unreleased]

## [0.3.0] - 2026-09-07

### Added

- A common `Sampler`, `Sample`, and `SampleDelta` API for cumulative measurements and differences between samples.
  The default scope is the calling native thread on every platform. CPU time is available on Linux, Windows, and macOS;
  Windows and macOS also support explicit process scope. See the [sampler support matrix](docs/SAMPLER_API.md#support-matrix).
- The Linux sampler uses `perf_events` for current-thread CPU time, page faults, context switches, CPU cycles, and
  retired instructions, subject to event availability. Events include kernel execution, so the sampler requires
  kernel-inclusive perf permissions; permission failures throw `IOException`. Hardware counts are scaled for
  multiplexing. Linux process scope is unsupported.
- PIE package metadata and installation from a local Linux source checkout.
- An experimental low-level Windows x64 backend for process and thread cycle counts, process CPU time and page faults,
  and current-thread profiling data, with immutable typed result objects.
- Experimental macOS process and current-thread resource snapshots on Apple Silicon with ARM64 PHP.
- Structured `scope` and `unsupportedMetrics` metadata on `UnsupportedMetricException`, allowing callers to retry
  sampler requests without parsing exception messages.
- An idempotent `Handle::close()` method for deterministic release of owned Linux performance-counter descriptors;
  closing a borrowed request handle detaches only that wrapper.
- `ClosedException`, `WrongThreadException`, and `ResourceBusyException` distinguish lifecycle misuse, Windows
  current-thread misuse, and thread-profiling conflicts from native `IOException` failures.
- `Metric::unit()` exposes each sampler metric's `MetricUnit`, allowing generic reporters to distinguish nanoseconds
  from unitless counts without hard-coded metric lists.
- The macOS sampler probes process CPU-cycle accounting when opened and rejects cycle requests when the host
  provides no usable counter.

### Changed

- All platforms now require 64-bit PHP, enforced during native compilation.
- The Composer package type is now `php-ext`, allowing PIE to recognize this package as `ext-perfidious`.
- `Handle::rawStream()` now declares its existing optional descriptor index (`0` is the event-group leader) in
  reflection and stubs. Invalid indices throw `ValueError` instead of returning `null`. Its `resource` return is
  documented in PHPDoc rather than native reflection metadata, which PHP cannot represent safely.
- Runtime reflection and the shipped, platform-specific PHPStan declaration set now expose the same public contract.
  The top-level `perfidious.stub.php` remains a declarative all-platform compatibility stub, while platform-specific
  PHPStan configurations expose only the APIs available on their selected platform.
- `Perfidious\open()` now declares its `$pid` and `$cpu` parameters as non-nullable integers, matching the existing
  runtime parser and their respective `0` and `-1` defaults.
- Linux perf-event reads now always throw `OverflowException` when a native counter or timing field cannot fit in a
  PHP integer.
- Resetting an enabled Linux handle briefly pauses counting and resumes it after the reset. Public reads retain
  kernel-lifetime enabled/running timing totals.

### Removed

- The Linux `Perfidious\global_handle()` API and the `perfidious.global.enable` / `perfidious.global.metrics` INI
  settings. This is a breaking change: automatic cumulative counters across requests are no longer provided. The
  per-request handle and explicitly owned handles remain available.
- Configurable overflow modes: `OVERFLOW_THROW`, `OVERFLOW_WARN`, `OVERFLOW_SATURATE`, `OVERFLOW_WRAP`, and
  `perfidious.overflow_mode`. Callers must handle `OverflowException` instead of selecting warning, saturation, or wrap
  behavior.
- The obsolete PECL `package.xml` manifest. Use PIE or the documented source build.

### Fixed

- Linux request metric lists now share the 1000-name limit of `Perfidious\open()`, bounding the temporary pointer array.
  Oversized lists report `OverflowException` when `request_handle()` is called.
- Linux `Perfidious\get_pmu_event_info()` now rejects mismatched PMU/event pairs.
- Linux `Perfidious\open()` now accepts referenced event strings, including arrays processed by reference iteration.
- Linux handle opening checks both PID conversion bounds and requires a CPU ID of `-1` or a nonnegative native integer.
  CPU IDs are no longer limited by the number of online CPUs, which could reject valid IDs on sparse CPU topologies.
- Linux PMU metadata lookups reject out-of-range PMU and event identifiers before native integer conversion,
  and error messages preserve the original PHP integer.
- Linux counter descriptors and raw streams now close automatically when a child executes another program.
  This requires kernel support for `PERF_FLAG_FD_CLOEXEC`, introduced in Linux 3.14.
- Linux capability checks release their libcap allocation when opening a handle for a positive PID, fixing a memory leak.
- Unix configure now honors debug, coverage, and sanitizer options when run by Dash or another POSIX shell.
- Linux handle construction establishes PHP cleanup ownership before acquiring native resources and releases partially
  opened event groups on failure.
- Linux phpinfo request-counter scaling now uses enabled/running times since the latest reset, so earlier requests'
  scheduling ratios do not affect the current estimate. Public reads retain kernel-lifetime timing totals.
- Automatic Linux request counters now open in the serving worker, including after opcache preloading, and are
  released when their module globals are destroyed. Initialization and lifecycle errors are deferred to
  `request_handle()` and delivered once; failed opens are retried on later requests.
- Linux phpinfo counter scaling now uses unsigned 128-bit intermediates when available, avoiding overflow in
  `counter * timeEnabled / timeRunning` when the final scaled value still fits in 64 bits.

### Security

- Updated PHP_CodeSniffer to 3.13.6 to address CVE-2026-67434.

## 0.2.0 - 2026-08-01

### Added

- Support for PHP 8.5.
- A NixOS VM test running the extension under real php-fpm requests (`nix build
  .#*-vmtest`), covering `perfidious.global.enable` / `perfidious.request.enable`
  persistence across many requests handled by the same worker process - something the
  CLI-only `.phpt` suite can never exercise, since every CLI invocation only ever sees a
  single "request".
- A static-linked ASan/UBSan build (`nix build .#sanitize-static-php82` /
  `.#sanitize-static-php82-check`) for sanitizer testing. Opt-in only, since it rebuilds
  all of PHP core from source.

### Changed

- Relicensed from `AGPL-3.0-or-later` to `AGPL-3.0-only WITH romic-exception`. The Romic Exception
  is a linking exception: it permits this extension to be linked or combined with other code (e.g.
  the PHP applications that load it) without that other code becoming subject to the AGPL merely
  because of the linking. Modifications to the extension itself remain fully AGPL. See
  [`docs/LICENSE_EXCEPTION.md`](docs/LICENSE_EXCEPTION.md) and [`CONTRIBUTING.md`](CONTRIBUTING.md)
  for the complete terms, including the new CLA-based contribution model.
- `Perfidious\open()` now rejects more than 1000 event names with `Perfidious\OverflowException`,
  instead of accepting an unbounded array.

### Fixed

- Fixed a memory leak: the handle's underlying native struct was never freed when a `Handle` was
  closed or garbage collected. Most noticeable for the persistent handles enabled via the
  `perfidious.global.enable` / `perfidious.request.enable` ini settings, which leaked once per
  process.
- `Handle::rawStream()`: closing the returned stream no longer closes the handle's own file
  descriptor out from under it. Previously this broke all further use of the `Handle` (`read()`,
  `enable()`, etc.) after the stream was closed.
- `-Werror` no longer defaults on for a plain `git clone` + `phpize && ./configure` build -
  only inside the project's own `nix develop` shell now. Previously any git checkout defaulted
  to fatal warnings, which could hard-fail a build over a warning that's harmless on our own
  compilers but not on someone else's.

### Security

- Fixed a stack buffer over-read that could leak adjacent stack memory into a `PmuEventInfo`
  object's `$name` property, for PMU/event name combinations long enough to overflow an internal
  formatting buffer.
- Fixed a stack buffer over-read that could leak adjacent stack memory into exception and warning
  messages, for event names long enough to overflow an internal formatting buffer.

## 0.1.0 - 2024-04-07

### Added

- Initial release

[Unreleased]: https://github.com/jbboehr/php-perfidious/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/jbboehr/php-perfidious/compare/v0.2.0...v0.3.0
