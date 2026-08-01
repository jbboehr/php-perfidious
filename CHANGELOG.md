# Changelog

All Notable changes to `php-perfidious` will be documented in this file.

Updates should follow the [Keep a CHANGELOG](http://keepachangelog.com/) principles.

## [Unreleased]

### Added

- Support for PHP 8.5.

### Changed

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

### Security

- Fixed a stack buffer over-read that could leak adjacent stack memory into a `PmuEventInfo`
  object's `$name` property, for PMU/event name combinations long enough to overflow an internal
  formatting buffer.
- Fixed a stack buffer over-read that could leak adjacent stack memory into exception and warning
  messages, for event names long enough to overflow an internal formatting buffer.

## 0.1.0 - 2024-04-07

### Added

- Initial release

[Unreleased]: https://github.com/jbboehr/php-perfidious/compare/v0.1.0...HEAD
