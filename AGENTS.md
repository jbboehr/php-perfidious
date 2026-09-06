# Repository guidance

## Adopted guides

This repository adopts Ruinenwert and The Measure of Words from
[`jbboehr/doctrine-of-the-second-sun`](https://github.com/jbboehr/doctrine-of-the-second-sun).
Composer installs the guides as a development dependency; `composer.lock` pins the reviewed revision.

Run `composer install --prefer-dist --no-progress` if the guides are missing. Read and apply:

- [Ruinenwert](vendor/jbboehr/doctrine-of-the-second-sun/RUINENWERT.md) for engineering work across source, tests,
  public contracts, build tooling, and development documentation. Adoption includes the engineering baseline and
  Fork Continuity.
- [The Measure of Words](vendor/jbboehr/doctrine-of-the-second-sun/MEASURE-OF-WORDS.md) for technical documentation,
  comments, docblocks, commit messages, pull requests, issues, and review findings.

Project-specific instructions govern local scope, compatibility, and verification. Apply the two guides within those
boundaries. Review changes to the adopted guides when updating the locked dependency, together with any affected local
policy.

## Project context

Before structural changes, read the relevant public documentation, contracts, and tests:

- [README](README.md): installation, configuration, and public usage.
- [Sampler API](docs/SAMPLER_API.md): metric semantics, lifecycle, errors, and platform support.
- [Development guide](docs/development/guide.md): local builds, generation, static analysis, PHPTs, and platform checks.
- [Project review](docs/development/project-review.md): experimental evidence, accepted decisions, and unresolved limits.

The platform declarations in `stubs/` are canonical. Regenerate `perfidious.stub.php` with
`php tools/generate-aggregate-stub.php` after changing them. Preserve public behavior with tests through the PHP API;
use native fixtures for failure paths that cannot be exercised reliably through live platform calls.

Keep maintainer workflows and implementation notes in `docs/development/`. Keep `README.md` focused on end users.

## Verification

Use the commands in the development guide. Run checks appropriate to the change, including focused tests and the full
suite for runtime changes. Report what ran, skips, and unverified platform behavior. A platform shim does not establish
native platform correctness.

Keep changes narrow and leave them for review before committing unless the user has authorized the commit.
