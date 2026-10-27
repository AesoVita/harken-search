param(
    [string]$ArisExe = "",
    [int]$WebPort = 5173,
    [int]$BridgePort = 17891,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$zoneDirectory = Join-Path ([IO.Path]::GetTempPath()) "ArisAnnotationBridge"
$zonePath = Join-Path $zoneDirectory "zones.json"
$annotationUrl = "http://127.0.0.1:$WebPort/?desktop-annotation=1&bridge=$BridgePort"
$bridgeHealthUrl = "http://127.0.0.1:$BridgePort/health"
$webHealthUrl = "http://127.0.0.1:$WebPort/?desktop-annotation=1"

function Test-HttpReady([string]$Url) {
    try {
        $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
        return $response.StatusCode -eq 200
    } catch {
        return $false
    }
}

if (-not $ArisExe) {
    $candidates = @(
        (Join-Path $repoRoot "build_dbg\Debug\HarkenSearch.exe"),
        (Join-Path $repoRoot "build\Release\HarkenSearch.exe")
    )
    $ArisExe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $ArisExe -or -not (Test-Path -LiteralPath $ArisExe)) {
    throw "Aris executable not found. Build Debug or pass -ArisExe explicitly."
}

if ($DryRun) {
    Write-Output "ARIS=$ArisExe"
    Write-Output "ZONE_FILE=$zonePath"
    Write-Output "BRIDGE=http://127.0.0.1:$BridgePort"
    Write-Output "PAGE=$annotationUrl"
    return
}

$existingAris = Get-Process -Name "Aris", "Auro" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existingAris) {
    $zoneIsFresh = (Test-Path -LiteralPath $zonePath) -and
        ((Get-Date) - (Get-Item -LiteralPath $zonePath).LastWriteTime).TotalSeconds -lt 2.0
    if (-not $zoneIsFresh) {
        throw "Aris is already running without a fresh annotation export. Close it normally, then run this launcher again."
    }
} else {
    $previousAnnotationValue = $env:ARIS_ANNOTATION_BRIDGE
    try {
        $env:ARIS_ANNOTATION_BRIDGE = '1'
        Start-Process -FilePath $ArisExe -WorkingDirectory (Split-Path -Parent $ArisExe)
    } finally {
        $env:ARIS_ANNOTATION_BRIDGE = $previousAnnotationValue
    }
}

if (-not (Test-HttpReady $webHealthUrl)) {
    Start-Process -FilePath "npm.cmd" `
        -ArgumentList @("--prefix", "src/webui", "run", "dev", "--", "--host", "127.0.0.1", "--port", "$WebPort") `
        -WorkingDirectory $repoRoot `
        -WindowStyle Hidden
}

if (-not (Test-HttpReady $bridgeHealthUrl)) {
    $pythonCommand = (Get-Command python -ErrorAction Stop).Source
    Start-Process -FilePath $pythonCommand `
        -ArgumentList @("tools/aris_annotation_server.py", "--host", "127.0.0.1", "--port", "$BridgePort", "--zones", $zonePath, "--title", "Aris") `
        -WorkingDirectory $repoRoot `
        -WindowStyle Hidden
}

$ready = $false
for ($attempt = 0; $attempt -lt 60; $attempt++) {
    try {
        $healthResponse = Invoke-WebRequest -Uri $bridgeHealthUrl -UseBasicParsing -TimeoutSec 2
        $health = $healthResponse.Content | ConvertFrom-Json
        if ($healthResponse.StatusCode -eq 200 -and $health.ok -and $health.synchronized) {
            $ready = $true
            break
        }
    } catch {
    }
    Start-Sleep -Milliseconds 500
}
if (-not $ready) {
    throw "Annotation bridge did not synchronize. Check the Aris window, zone export, and bridge service."
}

Start-Process $annotationUrl
Write-Output "Opened $annotationUrl"
