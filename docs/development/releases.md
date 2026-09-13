# Releases

Update the version metadata, generated declarations, release date, and changelog. Push the prepared commit to
`release` or a `release/**` branch to build a draft release before creating a tag:

```sh
git switch -c release/v0.3.1
git push -u origin release/v0.3.1
```

After CI passes, inspect the draft and its ZIPs on GitHub's Releases page. The draft uses the version from the header
(`v0.3.1` in this example) without creating the Git tag. Each successful release-branch run refreshes its assets and
links to the changelog at the exact build commit. Branch runs refuse to change an already published release.

When ready, replace `COMMIT_SHA` below with the commit you inspected and push the matching tag:

```sh
git tag -a v0.3.1 COMMIT_SHA -m "v0.3.1"
git push origin v0.3.1
```

The tag run rebuilds and checks the packages, then publishes the existing draft. Tagging directly after CI passes on
`develop` or `master` also works; preparing a draft first is optional.

## Packages and CI

[CI](../../.github/workflows/ci.yml) builds these packages for PHP 8.1–8.5:

| Platform | Architecture | Thread safety | Minimum platform |
| --- | --- | --- | --- |
| Windows | x64 | NTS and TS | Matching PHP runtime requirements |
| macOS | ARM64 | NTS and ZTS | macOS 11.0; PHP may require a newer OS |
| Linux | x64 | NTS | glibc 2.36+ or musl 1.2.5+ |

One CI job reads `PHP_PERFIDIOUS_VERSION`, rejects a mismatched tag, and supplies the version to the package jobs.
Windows uses [PHP's Windows builder](https://github.com/php/php-windows-builder#workflow-for-tags). Linux and macOS
use Nix, then load the extracted modules and run PHPTs with PHP installed outside Nix. Each ZIP is built once per run.
Platform-specific branches retain their Windows-only or macOS-only job selection.

Package checks cover licenses, the extension version, PHP thread-safety mode, and a CPU-time sampler read.
Linux may skip the sampler read when perf access is denied; PMU enumeration must still succeed.
The macOS test command explicitly selects the installed PHP executable so tests cannot silently use a missing CLI.

After required CI checks pass, [the publication workflow](../../.github/workflows/publish.yml) collects the versioned
ZIPs from that same run. This happens on branches, pull requests, and tags, and fails if no ZIPs were downloaded.
Release-branch pushes create or update a draft and upload the ZIPs. Version-tag pushes publish after uploading their
checked ZIPs. Updates for the same version [queue](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency#example-queueing-multiple-pending-runs)
to prevent overlapping uploads. Failed, cancelled, or skipped
prerequisites prevent either operation; coverage-report finalization (`finish`) is not a prerequisite.
The publication job does not rebuild packages.

After publication, fresh jobs use PIE to install all thirty package configurations and check the loaded extension.
Linux and macOS must select a prebuilt binary. A VCS repository points PIE at this project without waiting for
Packagist to discover the tag. These release-asset installation checks run only after a tag is published.

Rerun failed jobs after fixing a build or upload failure. A failed PIE check leaves the release published and requires
investigation. If GitHub release immutability is enabled, published ZIPs cannot be overwritten.

To check an existing release with the current PIE workflow, run it manually from `master`:

```sh
gh workflow run publish.yml --ref master -f tag=v0.3.1
```

Manual runs check all thirty PIE configurations and skip the publication job. They do not rebuild or upload assets.
The manual trigger must be present on the default branch. Rerunning an old tag run uses its original workflow revision.

GitHub supplies source archives for each tag. Linux ARM64, Linux ZTS, debug builds, and other unmatched Unix builds
use PIE's source fallback; configure options also select source installation. Users on older Linux systems must
request a source build explicitly because PIE does not check libc versions. See the [PIE installation guide](../../README.md#pie).

## Build packages locally

On Apple Silicon, use the [Darwin release derivation](../../nix/release-darwin.nix):

```sh
nix build -L .#release-php81-darwin-nts
nix build -L .#release-php81-darwin-zts
```

The derivation runs PHPTs, removes runtime search paths, refreshes the ad-hoc signature, and checks that the module
loads. The [packager](../../nix/package-darwin.sh) requires ARM64, a macOS 11.0 deployment target, and system libraries
only. ZIPs contain `perfidious.so`, `LICENSE.md`, and `LICENSE_EXCEPTION.md` with fixed timestamps and no host metadata.
The Nix build tools may require newer macOS than the module; CI uses macOS 15.

On x64 Linux, use the [Linux release derivation](../../nix/release-linux.nix):

```sh
nix build -L .#release-php81-glibc
nix build -L .#release-php81-musl
```

Replace `php81` with `php82` through `php85` for other PHP versions. ZIPs appear under `result/`.
Linux packages statically link libcap/libpfm, hide their symbols, and remove Nix runtime paths. Packaging rejects
unexpected dependencies, Nix store references, and glibc requirements above 2.36. Each ZIP includes the module,
project licenses, dependency source archives, and dependency version/license information in `dependencies/README.txt`.

Test a Linux ZIP outside Nix with Docker:

```sh
nix build -L .#release-php81-glibc
docker run --rm -v "$PWD:/source:ro" -v "$(readlink -f result):/packages:ro" \
    --cap-add CAP_PERFMON \
    -e EXPECTED_VERSION=0.3.1 -e EXPECTED_PHP_VERSION=8.1 php:8.1-cli-bookworm \
    sh /source/tools/test-linux-release.sh \
    /packages/php_perfidious-v0.3.1_php8.1-x86_64-linux-glibc-nts.zip
```

Use the corresponding `alpine3.22` image and musl ZIP for musl. CI runs both variants without mounting `/nix/store`
or installing libcap/libpfm separately.

## Packaging fixtures

With PHP, Python 3, Bash, and Zip available, run the macOS fixtures; use Nix for the Linux fixtures:

```sh
python3 nix/package-darwin.py
nix build -L .#checks.x86_64-linux.release-packaging
```

Linux fixtures also run as `python3 tests/package-linux.py` with a native glibc C compiler, Binutils, and PatchELF.
The macOS fixtures use controlled `lipo`/`otool` output and real ZIPs; they do not establish native Mach-O compatibility.

## Build cache

CI prepares one shared Nix dependency cache for each host system: Linux x64 and macOS ARM64. The Linux matrix job and
the macOS dependency job are the only writers; build jobs wait for their host's preparation job and only restore caches.
Keys include the host OS, architecture, and a hash of `flake.lock`, `flake.nix`, and `nix/*.nix`.

Writers check for an existing cache without downloading it. On a miss, `nix build .#ci-dependencies` prepares compiler
and PHP dependencies without building Perfidious. The Linux cache also includes Windows SDK and lint dependencies.
If preparation fails, the failure remains visible in CI and build jobs still attempt their own dependencies.
Failed preparation does not save a cache.

Nix garbage collection removes unneeded store paths before saving. Source-only commits reuse the dependency cache;
extension builds, tests, and packaging still run in their existing jobs. VM-specific dependencies are fetched separately.
Missing cache entries cause normal dependency builds. Caches remain branch-scoped; tag runs can read default-branch caches, subject to
[GitHub's cache access rules](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#restrictions-for-accessing-a-cache).

## Verification limits

Local Linux release builds and packaged-module tests passed, including checks in Debian and Alpine image filesystems
using Bubblewrap. All twenty Unix package targets evaluated with PHPT checks enabled. Packaging fixtures and local
workflow checks passed, including publication failure paths exercised with a stand-in `gh`.

Native Windows/macOS execution, macOS Nix builds and signing, the minimum macOS version, Docker execution, GitHub
cache transfers/publication, and PIE installation of published binaries still require live verification. Local workflow
fixtures do not verify GitHub scheduling or permissions.
