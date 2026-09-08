# Releases

Prepare the version, release date, generated declarations, and changelog before tagging. After the release commit
passes CI, push a `v`-prefixed version tag such as `v0.3.0`.

[The release workflow](../../.github/workflows/release.yml) runs on pushes to `master`, `develop`, and `v*` tags. It
builds and tests Windows x64 and macOS ARM64 packages for PHP 8.1–8.5 in both TS and NTS modes. Every run uses the
header's version for package names and loads each extracted module with the matching PHP. Package checks verify
the license text, extension version, thread-safety mode, and a CPU-time sampler read with no PHP INI loaded.

Branch runs retain the ZIPs as workflow artifacts and create no GitHub release. Tag runs reject tags that disagree
with `PHP_PERFIDIOUS_VERSION`. Once all twenty builds and package checks pass, a tag run creates a GitHub release,
uploads the binary ZIPs, and publishes it.
The release notes link to the tagged changelog.
No manual release creation or publication is required.

The workflow uses [PHP's Windows builder and release actions](https://github.com/php/php-windows-builder#workflow-for-tags).
Windows packages contain the combined license and exception text as `LICENSE`.

macOS jobs build and install from the checkout with PIE, then run the PHPT suite before packaging. The
[packaging script](../../tools/package-darwin.sh) checks that the module is ARM64-only, targets macOS 11.0, and links
only system libraries. It follows [PIE's binary naming convention](https://github.com/php/pie/blob/1.4.10/docs/extension-maintainers.md#pre-packaged-binary)
and includes `perfidious.so`, `LICENSE.md`, and `LICENSE_EXCEPTION.md` at the ZIP root. This
avoids distributing modules that depend on the runner's Homebrew library paths. The deployment target does not
replace the PHP runtime's own OS requirements.

Source archives are provided by GitHub for the same tag. Only the publication job has repository write permission.

After publication, fresh Windows and macOS jobs install the tagged version with PIE for all twenty configurations. They check
the loaded version, PHP thread-safety mode, and a CPU-time sampler read. A VCS repository points PIE directly at this
repository so verification does not depend on Packagist discovering the tag first.
Branch runs exercise packaging and module loading; PIE's release-asset discovery and installation run only after a tag
has been published. The macOS install check requires PIE to select the prebuilt binary, so a source fallback cannot
hide a missing or incorrectly named release asset.

The PIE metadata prefers prebuilt binaries with source downloads as a fallback. PIE keeps its separate Windows DLL
download method; Linux builds from source because this workflow does not publish Linux binaries. macOS source
builds remain available for unmatched PHP builds and requests with configure options.

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

Run the macOS packaging fixtures locally with PHP, Python 3, Bash, and Zip available:

```sh
python3 tests/package-darwin.py
```

These fixtures use controlled `lipo`/`otool` output and real archives. They cover accepted system dependencies,
rejected Intel/universal modules, Homebrew and `@rpath` dependencies, deployment-target mismatches, and tool failure.
They do not establish Mach-O loadability or native macOS compatibility.

Local verification passed all nine packaging fixtures; removing the architecture, deployment-target, or dependency
guard made its corresponding negative fixture fail. PIE 1.4.10 accepted all twenty expected asset names and
the root-level `perfidious.so` layout; its Windows download method remained unchanged. An isolated Linux PHP 8.1
source installation through PIE passed, followed by 74 PHPT passes and 44 skips. The skips covered unavailable
platform, debug, ZTS, OpCache, and perf prerequisites. Workflow lint, PHP checks, and documentation lint also passed.

[Release run 34188446793](https://github.com/jbboehr/php-perfidious/actions/runs/34188446793) built and installed the
extension through PIE on all ten macOS configurations. PHP 8.3/8.4 NTS then failed to compile the Darwin probe harness:
its Linux substitute Mach headers shadowed Apple's headers. The Mach shims now forward to the SDK on macOS, matching
the existing `libproc.h` shim. PHP 8.2–8.5 ZTS reported success without running PHPTs because the generated Makefile
could not find its configured CLI. The release workflow now passes the installed PHP executable explicitly.

One-off compiler fixtures reproduced the conflicting pthread declaration and missing Mach clock declaration before
the fix, then passed with controlled platform headers. Executing the workflow's test command against a Makefile with
a nonexistent PHP path previously ran no tests; it now runs the focused PHPT and propagates an intentional test failure.
Both affected PHPTs passed on Linux/PHP 8.1, and the full suite had 84 passes and 34 prerequisite/platform skips.
The standalone probe also compiled and ran with PHP 8.3/8.4 Linux headers. Composer validation, declaration freshness,
PHP_CodeSniffer, PHPStan, and configured lint hooks passed.

These fixes still need a native macOS CI rerun. GitHub publication, PIE installation of a published macOS binary,
and execution on the minimum macOS version remain unverified. Ordinary Windows and macOS CI builds remain separate
from this release workflow.
