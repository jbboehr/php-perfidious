# Changelog

All Notable changes to `php-perfidious` will be documented in this file.

Updates should follow the [Keep a CHANGELOG](http://keepachangelog.com/) principles.

## [Unreleased]

### Added

- PIE metadata, local-checkout installation support, and a CI smoke test that installs and loads the extension through
  PIE.
- An experimental low-level Windows x64 backend for process and thread cycle counts, process CPU time and page faults,
  and current-thread profiling data, with immutable typed result objects.
- A Darwin build foundation and macOS CI smoke coverage on Intel and ARM64, ahead of the low-level counter API.
- Structured `scope` and `unsupportedMetrics` metadata on `UnsupportedMetricException`, allowing callers to retry
  sampler requests without parsing exception messages.
- An idempotent `Handle::close()` method for deterministic release of owned Linux performance-counter descriptors;
  closing a borrowed global or request handle detaches only that wrapper.

### Changed

- The Composer package type is now `php-ext`, allowing PIE to recognize this package as `ext-perfidious`.
- `Handle::rawStream()` now accepts an optional descriptor index (`0` is the event-group leader) and throws
  `ValueError` for an invalid index instead of returning `null`. Its `resource` return is documented in PHPDoc rather
  than native reflection metadata, which PHP cannot represent safely.
- Runtime reflection and the shipped, platform-specific PHPStan declaration set now expose the same public contract.
  The top-level `perfidious.stub.php` remains a declarative all-platform compatibility stub, while platform-specific
  PHPStan configurations expose only the APIs available on their selected platform.
- `Perfidious\open()` now declares its `$pid` and `$cpu` parameters as non-nullable integers, matching the existing
  runtime parser and their respective `0` and `-1` defaults.
- On Darwin, the sampler now probes CPU-cycle accounting when opened and rejects cycle requests only when the host
  provides no usable counter. The low-level Darwin snapshot API remains unchanged.

### Fixed

- `perfidious.overflow_mode` now controls conversion of Linux perf-event metric values and timing fields as originally
  documented. Invalid configured values fail safe to the default throw policy, and `Handle::read()` and
  `Handle::readArray()` now declare their warning-mode `null` result accurately.

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

[Unreleased]: https://github.com/jbboehr/php-perfidious/compare/v0.2.0...HEAD
