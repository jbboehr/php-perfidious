# Releases

Prepare the version, release date, generated declarations, and changelog before tagging. After the release commit
passes CI, push a `v`-prefixed version tag such as `v0.3.0`.

[The release workflow](../../.github/workflows/release.yml) runs on pushes to `master`, `develop`, and `v*` tags. It
builds and tests Windows x64 packages for PHP 8.1–8.5 in both TS and NTS modes. Every run uses the header's version for
package names, checks the packaged license and DLL name, and loads the extracted DLL with the matching PHP. The
package check verifies the extension version, thread-safety mode, and a CPU-time sampler read with no PHP INI loaded.

Branch runs retain the ZIPs as workflow artifacts and create no GitHub release. Tag runs reject tags that disagree
with `PHP_PERFIDIOUS_VERSION`. Once all ten builds and package checks pass, a tag run creates a GitHub release,
uploads the DLL ZIPs, and publishes it.
The release notes link to the tagged changelog.
No manual release creation or publication is required.

The workflow uses [PHP's Windows builder and release actions](https://github.com/php/php-windows-builder#workflow-for-tags).
The combined license and exception text is included in each binary package as `LICENSE`. Source archives are provided
by GitHub for the same tag. Only the publication job has repository write permission.

After publication, fresh Windows jobs install the tagged version with PIE for all ten PHP configurations. They check
the loaded version, PHP thread-safety mode, and a CPU-time sampler read. A VCS repository points PIE directly at this
repository so verification does not depend on Packagist discovering the tag first.
Branch runs exercise packaging and DLL loading; PIE's release-asset discovery and installation run only after a tag
has been published.

If a build or upload fails, publication does not run; rerun the failed jobs after resolving the failure. A failed PIE
check leaves the release published and requires investigation. Publishing also makes the assets immutable when
GitHub release immutability is enabled, so a successful release cannot be repaired by overwriting its ZIPs.

## Verification

Local checks passed for workflow syntax, branch version preparation, tag-version rejection, license preparation,
ZIP extraction, and loaded-version/thread-safety rejection. Nine invalid ZIP fixtures covered missing, corrupt,
ambiguous, or mismatched packages and incomplete contents. Removing the license check made its negative fixture pass.
ZIP extraction and version checks used the local Linux module; they did not execute a Windows DLL.
The publication command was checked with a stand-in `gh`
executable, including failure propagation; it made no GitHub writes. Composer validation, declaration freshness,
PHP_CodeSniffer, PHPStan, configured lint hooks, and local documentation links also passed.

This workflow has not yet run. Local workflow validation does not exercise GitHub publication or
establish native Windows installation. Ordinary Windows CI builds remain separate from this release workflow.
macOS PIE support is a separate follow-up.
