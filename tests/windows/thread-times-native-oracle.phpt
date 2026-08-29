--TEST--
Perfidious Windows current-thread times identify the native calling thread
--EXTENSIONS--
perfidious
--SKIPIF--
<?php
require __DIR__ . '/../skipif-windows-only.inc';

if (!function_exists('shell_exec')) {
    die('skip: shell_exec is unavailable');
}

$probe = shell_exec(
    'powershell.exe -NoProfile -NonInteractive -Command "[Console]::Write(\'available\')"'
);
if (trim((string) $probe) !== 'available') {
    die('skip: Windows PowerShell is unavailable');
}
?>
--FILE--
<?php

$script = <<<'POWERSHELL'
param([int] $ObservedProcessId)

$process = Get-Process -Id $ObservedProcessId
$rows = @($process.Threads | ForEach-Object {
    try {
        [pscustomobject]@{
            creationTimeFiletime = $_.StartTime.ToFileTimeUtc().ToString(
                [System.Globalization.CultureInfo]::InvariantCulture
            )
            kernelTime100ns = $_.PrivilegedProcessorTime.Ticks
            userTime100ns = $_.UserProcessorTime.Ticks
        }
    } catch {
    }
})

ConvertTo-Json -InputObject $rows -Compress
POWERSHELL;

$scriptPath = sprintf(
    '%s\\perfidious-thread-times-%d-%s.ps1',
    sys_get_temp_dir(),
    getmypid(),
    bin2hex(random_bytes(8))
);
register_shutdown_function(static function () use ($scriptPath): void {
    @unlink($scriptPath);
});
file_put_contents($scriptPath, $script);

$times = Perfidious\Windows\get_current_thread_times();
$command = sprintf(
    'powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%s" -ObservedProcessId %d',
    $scriptPath,
    getmypid()
);
$threads = json_decode((string) shell_exec($command), true, flags: JSON_THROW_ON_ERROR);

if (isset($threads['creationTimeFiletime'])) {
    $threads = [$threads];
}

$matches = array_values(array_filter(
    $threads,
    static fn(array $thread): bool =>
        $thread['creationTimeFiletime'] === (string) $times->creationTimeFiletime
));
$native = $matches[0] ?? null;

var_dump(count($matches) === 1);
var_dump($native !== null && $native['kernelTime100ns'] >= $times->kernelTime100ns);
var_dump($native !== null && $native['userTime100ns'] >= $times->userTime100ns);
--EXPECT--
bool(true)
bool(true)
bool(true)
