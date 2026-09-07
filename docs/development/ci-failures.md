# CI failure investigation

Checked on 2026-09-07 after S03 was committed as `4bc1f53`. The changes improve failure diagnostics and move
macOS CI to Apple Silicon. The Linux Valgrind failure remains unresolved.

## Linux Valgrind test

The [PHP 8.4 debug job](https://github.com/jbboehr/php-perfidious/actions/runs/34165305069/job/101875059586)
passes its ordinary suite, then fails `tests/sampler/linux-perf-live.phpt` under Valgrind 3.22.0. It reports one failed
test and zero leaked tests. The same fixture failed under Valgrind in the
[previous PHP 8.4 run](https://github.com/jbboehr/php-perfidious/actions/runs/34162546500/job/101867145810) and an
[earlier PHP 8.3 run](https://github.com/jbboehr/php-perfidious/actions/runs/34158960211/job/101856569724).
The failure therefore predates S02 and S03 and is not confined to PHP 8.4.

The failing assertion is absent from the logs. CI's `cat tests/*.log` only checks the top-level directory, while this
test writes diagnostics under `tests/sampler/`. That command then fails because its glob matches no files. No test
diagnostic artifact was uploaded. The Linux matrix's fail-fast setting cancels the remaining jobs after this failure.

Local reproduction on Linux x86-64 with PHP 8.1.34 did not fail: the focused PHPT passed under both Valgrind 3.26.0 and
3.22.0. The native fixture also passed three direct runs under 3.22.0, and instrumented 3.26.0 runs passed normally and
on one CPU. The ordinary suite passed 84 tests with 34 skips. These results do not establish the cause on GitHub's
Ubuntu runner. The next diagnostic run must capture the actual assertion before changing the fixture or sampler.

## Intel macOS PHP installation

The [macOS PHP 8.1 job](https://github.com/jbboehr/php-perfidious/actions/runs/34162546500/job/101867145864)
fails inside `setup-php`, before configuration or compilation, after roughly 32 minutes. The installer reports only
`Could not setup PHP 8.1`; its Homebrew output is redirected away.

The action maintainer has [announced the end of Intel macOS support](https://github.com/shivammathur/setup-php/issues/1112).
The [PHP 8.1 formula rebuild](https://github.com/shivammathur/homebrew-php/commit/92592eeae09505d0a09b4db763f62e67bd153422)
also removed its Intel macOS bottle. The `macos-15-intel` job depended on an unsupported installation path.
The suppressed output prevents identification of the final Homebrew error in this run.

Official macOS support is now limited to Apple Silicon with ARM64 PHP. Both PHP 8.1 and 8.5 CI jobs use `macos-15`,
which GitHub [documents as ARM64](https://docs.github.com/en/actions/reference/runners/github-hosted-runners#standard-github-hosted-runners-for-public-repositories).
This follows the PHP installer's recommendation and avoids maintaining a separate Intel installation path.
Intel source builds remain untested and unsupported; no architecture guard was added.

## Diagnostics change

[CI](../../.github/workflows/ci.yml) now recursively prints `.log`, `.diff`, and `.mem` files with their paths in the
macOS, Linux test, and coverage jobs. An empty diagnostic directory succeeds without adding a second failure.

One-off Bash fixtures reproduced the original collector's missing nested logs and empty-directory failure. The command
from the updated workflow passed both cases and a mixed case with top-level logs, nested diffs, Valgrind reports,
spaces in paths, and an unrelated PHP file.

A parsed workflow comparison confirmed two macOS jobs, PHP 8.1 and 8.5 on `macos-15`, with their existing build and test
steps. Only the macOS matrix and diagnostic collectors changed. Configured lint hooks, Composer validation, declaration
freshness, PHPCS, all documented PHPStan checks, local link targets, and `git diff --check` passed.
These changes have not run on GitHub. Native macOS builds and runtime suites were not rerun
for this CI and documentation change.
