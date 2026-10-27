[CmdletBinding()]
param(
    [string]$WorktreeRoot = "",
    [int]$Port = 4320
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($WorktreeRoot)) {
    $WorktreeRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $WorktreeRoot = (Resolve-Path $WorktreeRoot).Path
}

$healthUrl = "http://127.0.0.1:$Port/health"
$reviewUrl = "http://127.0.0.1:$Port/"
$listening = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue

if ($null -eq $listening) {
    $pythonPath = (Get-Command "python" -CommandType Application | Select-Object -First 1).Source
    $arguments = @(
        "tools/aris_design_review_server.py",
        "--host",
        "127.0.0.1",
        "--port",
        [string]$Port
    )
    Start-Process -FilePath $pythonPath -ArgumentList $arguments -WorkingDirectory $WorktreeRoot -WindowStyle Hidden | Out-Null
}

$health = $null
for ($attempt = 0; $attempt -lt 30; $attempt++) {
    try {
        $health = Invoke-RestMethod -Uri $healthUrl -Method Get -TimeoutSec 1
        break
    } catch {
        Start-Sleep -Milliseconds 250
    }
}

if ($null -eq $health -or -not $health.ok -or -not $health.readOnly) {
    throw "Aris design review did not become ready at $healthUrl"
}

Write-Output "Aris design review ready: $reviewUrl"
Write-Output "Mode: read-only"
