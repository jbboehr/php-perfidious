# Historical documentation audit from a user's perspective

At the reviewed revision, the documentation explained the APIs more completely than it explained how to get started.
The main gaps were completing installation on each advertised platform and configuring the PHP process that would run
the examples. The sampler example, support matrix, units, and lifecycle reference provided a useful foundation.

Reviewed revision: `f91e65df44758589936f8fcc8dbfb2713ccfa803`.

**Status: historical.** The proposals, examples, and execution results below describe that revision. They are not
current action items. U07–U09 are superseded by the Linux perf backend and thread-scope default; the remaining proposals
require revalidation. Use the current [sampler guide](../SAMPLER_API.md), [support matrix](../SAMPLER_API.md#support-matrix),
and [canonical declarations](../../stubs/common.stub.php) for present behavior.

The primary reader is a PHP application developer who wants to measure a block of code or a request and understands
PHP, but does not already know libpfm, PHP extension build tooling, or native counter semantics. This was a documentation
walkthrough with executable checks, not a usability study with independent participants.

**Medium** means a missing step or condition can prevent installation, configuration, or successful use.
**Low** means avoidable navigation or interpretation work. Editorial judgments are identified separately from observed
command and runtime behavior.

## Findings

### U02: Advertised platforms do not have complete installation paths (medium)

Locations: [requirements and installation](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L14),
[package platform restriction](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/composer.json#L37).

A macOS reader sees support in Requirements, then a Linux-only PIE path and source instructions explicitly scoped to
Linux. There is no macOS installation route. Windows readers get three build-tool names, without a linked SDK setup
guide, the required PHP build match, the resulting DLL location, or how to load it. The macOS requirements bullet
describes available counters instead of installation prerequisites.

This is a documentation and CI comparison, not a demonstrated native build failure. The existing CI has native
Windows and macOS build paths. PIE's package metadata currently allows only Linux, so generic PIE support for macOS
does not make this package installable there through PIE.

Give each advertised platform a clear route: requirements, building, loading, and verification. A linked platform
installation page is sufficient. Explain Windows version/architecture/TS matching. Derive the macOS route from the
working build configuration and verify it on a native host before presenting it as tested.

Example macOS build subsection, proposed and not natively verified:

> Install the Xcode Command Line Tools, Autoconf, and a supported 64-bit PHP with development headers. Put matching
> `php`, `phpize`, and `php-config` commands on PATH. From the selected source checkout, build and check the module:

```sh
phpize
./configure --enable-perfidious --with-php-config="$(command -v php-config)"
make
php -n -d extension="$PWD/modules/perfidious.so" --ri perfidious
```

> After the module loads successfully, run `make install` with the permissions required by your PHP installation,
> add `extension=perfidious.so` to that installation's configuration, and perform the activation checks below.

Example Windows subsection, also proposed and not natively verified:

> Windows currently requires a source build. Follow the PHP SDK's
> [phpize extension-build instructions](https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2#building_pecl_extensions_with_phpize).
> Match the installed PHP version, x64 architecture, thread-safety mode, and compiler. In the configured SDK shell,
> with the matching PHP and development package on PATH, enter the extension checkout. For PHP installed in `C:\php`:

```bat
phpize.bat
configure.bat --enable-perfidious --with-prefix=C:\php
nmake
```

> Copy the resulting `php_perfidious.dll` from the build's output directory into that PHP installation's
> `extension_dir`. Enable it with `extension=php_perfidious.dll`, then run `php --ri perfidious` using the same PHP.

`C:\php` is an example installation directory. The SDK guide describes the output directory selected by build mode.

### U03: Installation ends without verifying the intended PHP runtime (medium)

Location: [source installation completion](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L61).

“Add the extension to your php.ini” and “restart the web server” leave the reader to identify the active configuration
and PHP service. A CLI user needs no service restart. An FPM user may need to reload FPM separately from the web server.
Machines with multiple PHP versions also need matching `php`, `phpize`, and `php-config`.

Add a short verification step:

```sh
php --ini
php --ri perfidious
```

Explain that these commands inspect CLI PHP, and that web users must check the configuration and loaded extension in
their serving PHP runtime as well. Distinguish manual source activation from PIE's attempt to enable the extension
automatically. [PHP configuration documentation](https://www.php.net/manual/en/configuration.file.php) describes
SAPI-specific configuration selection; [PIE's INI configuration guidance](https://php.github.io/pie/)
describes automatic activation and its failure fallback.

The module check returned exit status 1 without the extension and 0 when explicitly loading the local module.
This verifies the diagnostic, not an installation into system PHP. No service was restarted.

Example activation text:

> Use `php --ini` to locate CLI PHP's configuration, then `php --ri perfidious` to confirm the extension is loaded.
> A successful check prints the extension name and version. A new CLI invocation reads the updated configuration.
> For PHP-FPM, reload the PHP-FPM service after changing its configuration. For Apache's PHP module, restart Apache.
> Check the PHP runtime serving the application too: CLI success does not verify the web application's configuration.

A small diagnostic to run through the PHP runtime being checked:

```php
<?php
printf("PHP: %s (%s)\n", PHP_VERSION, PHP_SAPI);
printf("Main INI: %s\n", php_ini_loaded_file() ?: 'none');
printf("Additional INI files: %s\n", php_ini_scanned_files() ?: 'none');
printf("Perfidious loaded: %s\n", extension_loaded('perfidious') ? 'yes' : 'no');
```

For a web application, run this through its existing diagnostic mechanism.

### U04: Request-counter setup requires knowledge the example does not teach (medium)

Locations: [request example](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L161) and
[configuration table](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L260).

The example places INI settings in PHP comments. The later table labels them `PHP_INI_SYSTEM`, without explaining
where a user should set them or that `ini_set()` cannot enable request counting in an already running request.
Copying just the PHP example returns `NULL` with the defaults. That is correct behavior, but it gives a new reader
little help distinguishing disabled counters from a setup mistake.

Verified on PHP 8.1: the copied example returned `NULL`; `ini_set('perfidious.request.enable', '1')` returned
`false`; supplying the documented settings at process startup produced a `ReadResult` with all three requested
values.

Show actual INI content beside the example, state that it applies only to Linux, and explain that the settings must
be supplied before the PHP request starts. For a short demonstration:

```ini
perfidious.request.enable=1
perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u
```

Pair the settings with a complete CLI example, saved as `request-demo.php`:

```php
<?php
try {
    $handle = Perfidious\request_handle();
    if ($handle === null) {
        echo "Request counters are disabled or unavailable.\n";
        return;
    }
    foreach ($handle->readArray() as $event => $count) {
        printf("%s: %d\n", $event, $count);
    }
} catch (Perfidious\ExceptionInterface $error) {
    fwrite(STDERR, $error->getMessage() . PHP_EOL);
    exit(1);
}
```

With the extension already loaded, supply the settings at startup without editing system configuration:

```sh
php -d perfidious.request.enable=1 \
    -d perfidious.request.metrics=perf::PERF_COUNT_SW_CPU_CLOCK:u request-demo.php
```

Without those settings, the example prints the disabled/unavailable message. With them, it prints the event name and
count, or a diagnostic if perf access fails. Link to the existing error/retry semantics and retain the distinction
between a PHP request and a job in a long-running CLI worker.

### U05: Capability fallback advice omits the empty-result case (medium)

Location: [sampler fallback advice](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/docs/SAMPLER_API.md#L81).

The guide tells applications to remove unsupported metrics and retry. If every requested metric is unsupported,
following that instruction replaces `UnsupportedMetricException` with `ValueError`.

Verified on Linux using `[Metric::CpuCycles]`: removing the reported unsupported metric and reopening produced
`Argument #1 ($metrics) must not be empty`. A mixed request containing CPU cycles and CPU time successfully opened
after removing CPU cycles.

A complete example can treat every requested metric as optional and stop when none remains. Applications with
required metrics should report their absence instead of removing them. Native access and resource failures still
propagate; only unsupported metrics trigger this fallback.

```php
<?php
use Perfidious\Metric;
use Perfidious\Sampler;
use Perfidious\UnsupportedMetricException;

$metrics = [Metric::CpuCycles, Metric::CpuTime];
$sampler = null;
while ($metrics !== []) {
    try {
        $sampler = Sampler::open($metrics);
        break;
    } catch (UnsupportedMetricException $error) {
        $metrics = array_values(array_filter(
            $metrics,
            static fn (Metric $metric): bool => !in_array($metric, $error->unsupportedMetrics, true),
        ));
    }
}

if ($sampler === null) {
    echo "None of the requested metrics is available.\n";
    return;
}

try {
    $sample = $sampler->read();
    foreach ($sampler->metrics() as $metric) {
        printf("%s: %d\n", $metric->value, $sample->value($metric));
    }
} finally {
    $sampler->close();
}
```

On Linux, the mixed request falls back to CPU time. Requesting only `Metric::CpuCycles` prints the unavailable message.
`array_values()` preserves the documented list type. No change to the API's rejection of empty requests is needed.

### U07: Troubleshooting obscures the simpler Linux path and has an incomplete command (low)

**Superseded.** The Linux sampler now requires kernel-inclusive perf access, and permission failures raise
`IOException`. The perf-free sampling advice and experiment below describe the former backend.

Locations: [Linux sampler backend explanation](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/docs/SAMPLER_API.md#L163)
and [README troubleshooting](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L283).

The README gives Linux perf permission remedies, but the useful fact that the common Linux sampler uses
`getrusage()` is only in the detailed API guide. Readers choosing how to obtain CPU time or page faults should learn
that the common sampler does not require perf-event access, even when native perf counters are unavailable.

Experimentally forcing every `perf_event_open()` call to fail with `EPERM` left the README sampler example working.
Its trace contained `getrusage()` calls and no perf-event calls. The same restriction made the low-level handle example
fail with `IOException`. This was isolated to traced child processes; no kernel setting or capability was changed.

Label the configuration and perf troubleshooting material as Linux native-counter guidance and point readers to the
common sampler when it meets their needs. Explain the scope of any proposed kernel setting before readers change it.

The displayed `docker run --rm -ti --cap-add CAP_PERFMON` command also lacks an image argument. Running it returned
“requires at least 1 argument” before starting a container. Present `--cap-add CAP_PERFMON` as an option to add to the
reader's existing command, or show a complete command with a clearly identified image.

Example troubleshooting text:

> These perf-event permission checks apply to Linux native handles and automatic request counters. For current-process
> CPU time, page faults, or context switches, the common `Sampler` API works without perf-event access.
>
> For a container using native perf counters, add `--cap-add=CAP_PERFMON` before the image name in your existing
> `docker run` command. This grants the container an additional capability, and other host restrictions can still apply.

For an existing local image named `my-php-app` containing PHP and Perfidious, a complete module-check command is:

```sh
docker run --rm --cap-add=CAP_PERFMON my-php-app php --ri perfidious
```

The image name is illustrative. This command checks extension loading, not whether the host permits perf events.

### U08: The README needs a clearer route from purpose to first result (low, editorial)

**Superseded.** The default scope is now `CurrentThread`. Windows and macOS reject page faults in that scope, so the
quickstart below is not portable. The current [README example](../../README.md#cross-platform-sampler) selects CPU time.

Locations: [opening](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L11),
[usage](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L76),
[native detail](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L140), and
[contributor licensing detail](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/README.md#L364).

“Common sampler” and “low-level access” name components before showing the task they solve. The first useful example is
sound, but the page then spends substantial space on raw dumps, reset-scaling details, Windows native fields, and
contributor licensing routes. A new user needs to choose a measurement path and understand the result before those
details.

An opening such as “Measure CPU time, page faults, and other counters around a block of PHP code” would establish the
use case. Keep one common sampler example and explain CPU time versus elapsed time beside it, with a link to
`SampleDelta::$elapsedTimeNs`. Offer direct links for arbitrary Linux events and richer Windows/macOS snapshots.

Shorten repeated dumps, including the request dump's literal `#%d (%d)` test placeholders. Move extended native
reference details to linked user documentation. Retain the end-user license and exception explanation, linking to
CONTRIBUTING for the contribution routes.

The examples link also needs orientation: all seven checked-in scripts use Linux-specific APIs, and none is a
runnable common-sampler example. Add a sampler example with its run command and a short index describing each script's
purpose and platform. This is a discoverability recommendation, not a claim that the existing scripts are invalid.

Example opening and runnable quickstart, proposed for `examples/sampler.php`:

> Measure CPU time, page faults, and other counters around a block of PHP code. Start with the common sampler for
> process metrics on Linux, Windows, and macOS. Use the native APIs when you need arbitrary Linux events or additional
> platform-specific data.

```php
<?php
use Perfidious\Metric;
use Perfidious\Sampler;

$sampler = Sampler::open([Metric::CpuTime, Metric::PageFaults]);
try {
    $before = $sampler->read();
    $digest = hash('sha256', str_repeat('x', 1_000_000));
    $delta = $sampler->read()->since($before);

    printf("SHA-256: %s\n", $digest);
    printf("CPU time: %.3f ms\n", $delta->value(Metric::CpuTime) / 1_000_000);
    printf("Elapsed time: %.3f ms\n", $delta->elapsedTimeNs / 1_000_000);
    printf("Page faults: %d\n", $delta->value(Metric::PageFaults));
} finally {
    $sampler->close();
}
```

> With the extension enabled, run `php examples/sampler.php`. CPU time measures work performed by the process;
> elapsed time also includes waiting. Results vary by workload and host, and a zero page-fault delta can be normal.

Example entries for a short examples index:

| Script | Purpose | Platform |
| --- | --- | --- |
| `sampler.php` (proposed) | Measure a block of PHP code and compare CPU time with elapsed time | Linux, Windows, macOS |
| `all-events.php` | List libpfm events for selected PMUs; use `--help` for options | Linux |
| `three-sw-clock.php` | Read a software CPU-clock counter three times | Linux |
| `watch.php` | Watch native events for the selected PID/CPU | Linux |

Example reduction of the contributor-specific README text:

> For contribution terms and the optional CLA route, see CONTRIBUTING.md.

Retain the existing end-user license and exception explanation above that link.

### U09: The sampler reference mixes current use with implementation planning (low, editorial)

**Superseded.** Linux now implements current-thread perf sampling, including cycles and instructions, and rejects
`CurrentProcess`. The proposed backend below has been implemented; its old scope guidance must not be reused.

Locations: [backend mapping](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/docs/SAMPLER_API.md#L161) and
[future work](https://github.com/jbboehr/php-perfidious/blob/f91e65df44758589936f8fcc8dbfb2713ccfa803/docs/SAMPLER_API.md#L266).

The guide already puts usage first, provides a useful support matrix, and documents lifecycle and units. It then asks
application developers to navigate native backend mappings and a proposed Linux perf-event implementation, including
an event table for behavior they cannot currently use.

Keep current support, measurement limitations, lifecycle, errors, and necessary native semantics in the user reference.
Move the deferred implementation proposal and design rationale to development documentation, leaving a short link.
Do not remove qualifications about scope, wraparound, non-atomic snapshots, or unavailable counters; those affect how
users interpret measurements.

Example text to retain in the user reference:

> On Linux, the common sampler supports current-process CPU time, page faults, and context switches. Current-thread
> sampling, CPU cycles, and instruction counting are unavailable through that API. See the support matrix for the
> combinations available on each platform. The low-level Linux API remains available for arbitrary perf events.

The implementation proposal could move to a new `docs/development/sampler-design.md`, organized as:

```text
# Sampler backend decisions
## Current native mappings
## Proposed Linux current-thread backend
## Requirements for process-wide perf accounting
```

That filename is proposed; no new design guide or example script is created by this audit.

## Suggested reading order

For the README:

1. Purpose and a small set of navigation links.
2. One short common-sampler example, with units and a link to installation.
3. Requirements and installation by platform, ending with activation checks.
4. API selection links and Linux request-counter setup.
5. Troubleshooting and license.

Keep advanced native usage in linked user-facing guides. Keep build verification and design proposals in
`docs/development/`. CONTRIBUTING already links to the development guide near its start, and the project review now
separates current disposition from historical evidence; retain those improvements.

## Verification and limits

### Original documentation

- All seven PHP blocks in README and the sampler guide passed PHP 8.1 syntax checks.
- The common example in both documents, the Linux handle example, and the request example ran on Linux x86-64 with
  PHP 8.1.34. Request settings were tested both absent and supplied at startup.
- The empty fallback, guarded empty fallback, mixed-metric fallback, module-loading diagnostic, runtime INI rejection,
  isolated perf-denial comparison, and incomplete Docker command were exercised as described above.
- PHPStan checked the snippets against the appropriate platform declarations. Windows and Darwin runs passed.
  The Linux run reported two warnings for the deliberately discarded `hash()` workload, once in each copy of the
  common example. No API type errors were reported. Those workload warnings are not treated as runtime defects.
- All local Markdown links and anchors were checked. The report's source citations are pinned to the reviewed
  revision. The README's four-backtick closing fence is valid Markdown and is not a broken-rendering finding.
- CONTRIBUTING, the development guide, prior audits, and example entry points were included in the
  walkthrough. License and governance documents were considered for placement and navigation, not legal validity or
  changes to their chosen policy.
- No native Windows/macOS installation or execution, privileged installation, FPM service reconfiguration, real Docker
  run, or full runtime suite was performed for this documentation audit. Syntax and PHPStan checks do not establish
  native behavior. External links were checked selectively, not exhaustively.

### Proposed examples

- All four added PHP examples passed PHP 8.1.34 syntax checks and maximum-level PHPStan checks against their
  applicable Linux, Windows, and Darwin declarations. The request-counter example applies only to Linux.
- Ten Linux CLI runs exercised loaded/unloaded diagnostics, enabled/disabled/invalid request configuration,
  mixed/unsupported/supported/empty metric fallback, and the sampler quickstart. Expected results and exit statuses
  were checked, including error exit status 1 for an invalid request event.
- All four remaining Unix shell blocks passed Bash syntax checks. The Windows commands were checked against the linked PHP SDK
  instructions and project build configuration, but were not executed.
- The proposed macOS build/install, Windows build/install, service reload, and complete Docker
  command were not run. The application image name and installation directory are illustrative. No system
  configuration or installed extension was changed while testing these examples.
