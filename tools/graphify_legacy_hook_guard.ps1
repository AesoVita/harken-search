param(
    [switch]$Stop
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$pattern = [regex]::Escape($repo) + ".*graphify\.watch.*_rebuild_code\(Path\('\.'\)\)"

$procs = Get-CimInstance Win32_Process |
    Where-Object {
        $_.Name -match '^python(\.exe)?$' -and
        $_.CommandLine -match $pattern
    } |
    Select-Object ProcessId, Name, CommandLine

if (-not $procs) {
    Write-Output "No legacy graphify.watch rebuild hook processes found for $repo"
    exit 0
}

Write-Output "Legacy graphify.watch rebuild hook processes:"
$procs | Format-List

if ($Stop) {
    foreach ($proc in $procs) {
        try {
            Stop-Process -Id $proc.ProcessId -Force -ErrorAction Stop
            Write-Output "Stopped PID $($proc.ProcessId)"
        } catch {
            Write-Error "Failed to stop PID $($proc.ProcessId): $($_.Exception.Message)"
            exit 1
        }
    }
}
