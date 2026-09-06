# Documentation audit: The Measure of Words

All twelve findings are addressed in the documents and comments listed below. The edits distinguish current review
status from historical evidence, put user actions first, and remove repeated explanations while retaining API contracts
and experimental limits.

Reviewed revision: `e954804963b90325890c25ec2ce87716b4af7fdd`.
Standard: [The Measure of Words](../../vendor/jbboehr/doctrine-of-the-second-sun/MEASURE-OF-WORDS.md), installed by
Composer at `987702c25d9915d950857aef712d73880bdc0194`. Read the installed copy after `composer install`.

**Medium** means wording can mislead readers about an action, capability, or current status. **Low** means an editorial
improvement with less effect on meaning. Length alone is not a finding: contracts, reproducible evidence, qualifications,
and useful examples need space.

## Resolution

| Finding | Applied change |
| --- | --- |
| MW01 | [Project review](project-review.md#current-disposition) opens with delivered changes and remaining work; original findings are marked historical and source citations are pinned to their revision. |
| MW02 | [Installation](../../README.md#source) shows actual INI content. |
| MW03 | [Build comments](../../flake.nix) distinguish this VM fixture from PHPT coverage and extension instrumentation from PHP runtime linking. |
| MW04 | [Warning troubleshooting](../../README.md#troubleshooting) starts with the configure option and its effect. |
| MW05 | The [Unreleased entry](../../CHANGELOG.md#unreleased) describes the implemented Darwin snapshots. Dated release entries are preserved. |
| MW06 | [Evidence records](project-review.md#implementation-and-verification-records) retain decisions, run provenance, counts, and limits; temporary handoff narration is removed and repeated PHP check lists share one definition. |
| MW07 | [Sampler usage](../SAMPLER_API.md#usage) precedes a compact signature and enum reference. |
| MW08 | The [sampler guide](../SAMPLER_API.md#support-matrix) separates support, native rationale, and future requirements, using links for repeated explanations. |
| MW09 | [Zero-counter troubleshooting](../../README.md#troubleshooting) starts with reducing or separating the event group; the Zen4 observation remains host-specific. |
| MW10 | [Linux metadata docblocks](../../stubs/linux.stub.php) are shorter and grammatically corrected; the aggregate stub is regenerated. |
| MW11 | [Example introductions](../../examples) state the workload, and the overhead comment names `readArray()`. |
| MW12 | [CI labels](../../.github/workflows/ci.yml) identify the Debian and Fedora container tests. |

## Original findings

The findings below describe the reviewed revision above. Location links are pinned to that revision so they continue
to show the text that prompted each finding. Recommendations are retained as the rationale for the applied changes.

### MW01 — Medium: the review report mixes current and historical status

**Locations:** [opening](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L3), [evidence overview](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L1209),
[R11 status](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L1223), and [unresolved questions](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L1700).

The report starts with the original revision and review history, then places its overview after 1,200 lines of
follow-ups. Several statements still describe completed work as pending: R11 has only a source concern in the overview,
the exception-documentation issue remains open, and the reader is told to add the development guide that already exists.
Eleven notices say changes remain uncommitted for review. The historical disclaimer does not resolve the mixture of
updated and original statuses in the overview and unresolved-work sections.

Put a current disposition table and the remaining work first. Mark the original findings and experiments as historical,
with their exact revision. Replace session-status notices with commit references where needed. For example:

> R11: fixed by bounding request metric lists. Debug and release regressions passed; allocation-bailout behavior remains
> unverified. See the R11 evidence record.

Keep the original failures, test counts, and qualifications attached to the revisions where they were observed.

### MW02 — Medium: the README labels a shell command as INI content

**Location:** [README installation](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/README.md#L68).

“Add the extension to your php.ini” is followed by an `ini` block containing
`echo extension=perfidious.so | tee -a /path/to/your/php.ini`. Readers must infer whether to paste the block into a file
or execute it. Show the actual file content:

```ini
extension=perfidious.so
```

**Checked:** PHP 8.1's `parse_ini_string()` interpreted the audited block as a key named `echo extension`, with the pipe
and command arguments in its value. The corrected block produces the intended `extension` key. No configuration file
was changed and no service was restarted.

### MW03 — Medium: build comments overstate test and sanitizer coverage

**Locations:** [FPM fixture explanation](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/flake.nix#L263) and
[sanitizer explanation](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/flake.nix#L495).

The FPM comment says the CLI PHPT suite “never asserts real counter values” and calls its own lifecycle coverage
“fully deterministic regardless of PMU/timer virtualization quirks.” The current
[positive-counter test](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/tests/handle/non-zero-after-enable.phpt#L16) checks a real value, and PHPTs also launch FPM
fixtures. Scope the explanation to this VM check:

> This VM check exercises request-handle reuse and reset/enable/disable across requests in one FPM worker. It avoids
> counter-magnitude assertions because nested virtualization can prevent counters from advancing.

“Whole-process ASan/UBSan build” can imply that PHP core is instrumented. The
[build flags](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/flake.nix#L529) and [development guide](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/guide.md#L197) distinguish extension instrumentation from
linking sanitizer runtimes into PHP. Use that distinction immediately:

> Build Perfidious into PHP to avoid dynamic extension loading. Instrument Perfidious's objects and link the sanitizer
> runtimes into PHP; PHP core is not instrumented. These opt-in targets rebuild PHP from source.

Keep the target names and loader rationale. These findings come from local source and documentation comparison;
sanitizer or VM execution was not part of this writing audit.

### MW04 — Medium: warning troubleshooting buries the action and uses an ambiguous pronoun

**Location:** [README compiler-warning answer](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/README.md#L342).

The answer begins with development policy and compiler history. It ends with “disable it yourself,” where “it” could
mean warnings or treating warnings as errors. Lead with the action and name its effect:

> Use `./configure --enable-compile-warnings=yes` to keep warnings non-fatal. Nix-shell builds default to fatal warnings;
> plain source builds do not. Report unexpected fatal warnings outside Nix, including the compiler diagnostic.

The distinction follows the `IN_NIX_SHELL` branch in [config.m4](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/config.m4#L89). Keep any existing configure
options when applying the suggested override.

### MW05 — Medium: an unreleased change still describes an intermediate milestone

**Location:** [Changelog, Darwin addition](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/CHANGELOG.md#L15).

“A Darwin build foundation ... ahead of the low-level counter API” describes a stage of development. The current
[Darwin declarations](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/stubs/darwin.stub.php#L24) already expose process and thread resource snapshots. An unreleased
entry should describe what the release delivers:

> Experimental macOS process and current-thread resource snapshots, with Intel and ARM64 CI smoke coverage.

Check the final release diff when updating the entry. Preserve dated release history, including references to APIs that
existed in those releases; their later removal does not make the historical entries wrong.

### MW06 — Low: routine review narration obscures decisions and evidence

**Locations:** [R04 review follow-up](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L443),
[R06 review follow-up](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L596), and [R07 review follow-up](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/development/project-review.md#L680).

Seven passages mention the temporary `tmp.md` handoff. Several repeat that it was read alongside an independent review,
that no production change was needed, and that the handoff was removed. Those actions rarely add durable evidence.
Retain the decision and its reason instead. For the R07 include-order discussion:

> No include-order change: every current Linux caller includes `pfmlib.h` before `src/private.h`, and the dependency
> predates this patch. No affected caller was found.

Group repeated verification lists into compact records keyed by revision and environment. Preserve test counts, skips,
commands needed to reproduce experiments, reviewer-versus-local provenance, and reruns that followed test changes.
Do not collapse different runs into a single implied verification result.

### MW07 — Low: declaration scaffolding delays the usable sampler example

**Location:** [Sampler API shape](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/SAMPLER_API.md#L36).

The reader passes a 106-line declaration block, including empty method bodies and exception classes, before reaching
the first usage example. Most signatures already exist in the linked canonical stub. Put the working example first,
then a compact operation/signature reference or a link to the stub. Keep the enum values, defaults, return types, and
error metadata discoverable; do not shorten the document by hiding its public contract.

### MW08 — Low: sampler capability explanations recur without adding distinctions

**Locations:** [summary](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/SAMPLER_API.md#L13), [scope](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/SAMPLER_API.md#L192),
[support-matrix commentary](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/SAMPLER_API.md#L269), and [future work](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/docs/SAMPLER_API.md#L389).

Linux thread support is described as deferred in several places. The support matrix is followed by prose that restates
many of its unsupported cells. Phrases such as “no honest mapping” are less precise than the declared support state.

Keep acceptance in the matrix, semantics and rationale in Scope/Backend mapping, and future requirements in Future
work. Replace repeated Linux-thread paragraphs with a cross-reference. Preserve the distinct constraints: process-wide
perf accounting must include all threads, Windows hardware indices do not identify instructions by themselves, and
Darwin cycle availability requires a probe. Those reasons add information the matrix cannot carry.

### MW09 — Low: zero-counter troubleshooting leads with an anecdote

**Location:** [README zero-counter answer](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/README.md#L326).

The answer discusses an informal Zen4 observation and asks readers for more information before giving the useful action.
“For some reason” and “Note also” add no diagnostic detail. Start with:

> Reduce the events in the group or try separate handles. The kernel may be unable to schedule the entire group when
> hardware-counter capacity is insufficient. Rare events can also produce zero readings.

Keep capacity estimates explicitly host-specific if retaining them. The observed four-to-six counters must not become
a universal limit. This is a wording recommendation; the host observation was not repeated.

### MW10 — Low: PMU property docblocks repeat scaffolding and contain broken conditions

**Location:** [Linux metadata declarations](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/stubs/linux.stub.php#L173).

Several descriptions begin with “This is” or “This field is set to.” The `$pmu` comment contains “it provided,” and
`$nevents` contains “only valid is the is_present field.” These defects make otherwise short API descriptions harder
to read. For example:

```php
/** Symbolic PMU name, usable as an event-string prefix. */
public readonly string $name;

/** Whether this PMU model was detected on the host. */
public readonly bool $is_present;
```

Shorten `$equiv` around the equivalent event string it provides. Preserve type annotations and native-contract
qualifications; do not infer a new availability contract while correcting grammar. Edit the canonical stub and
regenerate `perfidious.stub.php` together.

### MW11 — Low: example introductions discuss themselves and name the wrong method

**Locations:** [event listing](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/examples/all-events.php#L20),
[overhead estimate](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/examples/estimate-overhead.php#L20), and the
[array](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/examples/sieve.php#L20) / [bit-string](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/examples/sieve2.php#L20) sieve examples.

“ok, these aren't all examples per se” adds no purpose or constraint. The overhead comment names `Handle::read()`, but
the measured loop calls `readArray()`. Suggested introductions:

```php
// List libpfm events for the selected PMUs.
// Estimate the overhead of Handle::readArray().
// Measure cycles and instructions for an array-based Sieve of Eratosthenes.
```

Give the bit-string version its corresponding description. These are separate per-file replacements, not one combined
header. Keep workload choices and measurement limitations that help interpret the results.

### MW12 — Low: CI step labels narrate frustration instead of identifying the task

**Locations:** [Debian step](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/.github/workflows/ci.yml#L525) and
[Fedora step](https://github.com/jbboehr/php-perfidious/blob/e954804963b90325890c25ec2ce87716b4af7fdd/.github/workflows/ci.yml#L560).

Both long labels begin “I am tired of writing” and explain the decision to use Bash. A failed CI step should identify
the operation that failed. Use `Run Debian container tests` and `Run Fedora container tests`. This changes the technical
labels, without requiring changes to the script, project names, or creative prose.

## Coverage and material to retain

At the reviewed revision, all 13 tracked Markdown files were inventoried. The table records their audit disposition.
The four canonical PHP stubs were reviewed as API documentation. A supplementary scan covered substantial source,
example, test, and build comments and CI labels; it was not a new code-correctness audit. Generated aggregate
declarations, third-party headers, vendored guides, and legal notices were not treated as independent opportunities to
rewrite prose.

| Document | Disposition |
| --- | --- |
| `README.md` | MW02, MW04, MW09; preserve metric units, lifecycle/error contracts, and platform limits |
| `AGENTS.md` | No actionable wording change; scope, dependency location, and verification are explicit |
| `CHANGELOG.md` | MW05; preserve version boundaries and historical API names |
| `docs/SAMPLER_API.md` | MW07, MW08; preserve timing, ownership, overflow, and snapshot qualifications |
| `docs/development/guide.md` | No actionable wording change; commands and prerequisites justify its length |
| `docs/development/project-review.md` | MW01, MW06; retain experiments and their provenance |
| `CONTRIBUTING.md` | Technical introduction is concise; operative contribution terms preserved |
| `.github/PULL_REQUEST_TEMPLATE.md` | Technical prompts are concise; licensing notice at the decision point preserved |
| `docs/STEWARD.md` | Already concise |
| `docs/CLA-v1.md` | Operative agreement preserved; no legal-equivalence or enforceability assessment |
| `docs/LICENSE_EXCEPTION.md` | Verbatim exception and explanation preserved |
| `LICENSE.md` | Verbatim upstream license excluded from editorial rewriting |
| `CODE_OF_CONDUCT.md` | Ceremonial governance text outside the guide's technical-writing scope |

The common, Darwin, and Windows stub docblocks generally state useful units, ownership, or native limitations. Their
repetition across independently accessed API entries is useful. MW10 concerns the older Linux property descriptions.
Likewise, the review report's shim/native distinction, allocation-bailout limits, and revision-specific test results
should survive any edit. Concision must not turn qualified evidence into a claim of complete verification.

## Initial audit verification

Findings were checked against the cited text and relevant local declarations, tests, and build configuration. A small
PHP parser experiment confirmed MW02. Text searches confirmed the repeated status notices and handoff references.
Editorial priorities and suggested wording remain review judgments, not test assertions.

The audit report passed Markdown lint, local-link and cited-line checks, and whitespace checks. Historical runtime
experiments were not rerun for the writing audit. Verification of the applied edits is recorded separately below.

## Resolution verification

Verification of the edits used Linux x86-64 and PHP 8.1.34. Comparisons used the reviewed revision above as their base.

- Strict Composer validation, aggregate-stub freshness, PHP_CodeSniffer, PHPStan, and all four declaration-analysis
  configurations passed. The declarations parsed, and the aggregate loaded without the extension.
- Configured Actionlint, Alejandra, Markdownlint, and ShellCheck hooks passed. The new audit file also passed
  Markdownlint. Local file links, heading fragments, and pinned source citations were checked; the pinned citations
  were validated against local Git objects. Whitespace checks passed.
- Executable PHP tokens are unchanged in all six modified PHP files. Parsed Nix expressions are unchanged, and the CI
  workflow differs only in the two step names. Dated changelog sections are unchanged.
- All 20 fenced examples and results in the project review are preserved, except that the historical `git archive`
  command now names its original revision. Numeric evidence remains in its individual implementation record. Diff
  review checked the retained decisions, reviewer-versus-local provenance, and platform and failure-path limits.
- Both sampler usage examples are unchanged and pass PHP syntax checks. The process example executed successfully on
  Linux. PHP parsed the corrected README INI block as the single `extension=perfidious.so` setting.

The full runtime suite, historical failure experiments, native Windows/macOS examples, VM and sanitizer targets, and
remote CI were not rerun for these documentation and comment edits. Existing verification gaps remain recorded in the
project review.
