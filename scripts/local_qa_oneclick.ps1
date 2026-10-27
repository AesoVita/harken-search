[CmdletBinding()]
param(
    [string]$WorkRepo = "E:/tmp/aris-local-qa-run",
    [string]$DistDir = "",
    [switch]$NoOpen
)

$ErrorActionPreference = "Stop"
$sourceRepo = Split-Path $PSScriptRoot -Parent
if (-not $DistDir) { $DistDir = Join-Path $sourceRepo "dist_qa" }
$dist = [IO.Path]::GetFullPath($DistDir)
$dataRoot = [IO.Path]::GetFullPath((Join-Path $dist "data"))
$remoteUrl = (& git -C $sourceRepo remote get-url origin).Trim()
if ($LASTEXITCODE -ne 0 -or -not $remoteUrl) { throw "Cannot read origin remote" }

$repo = [IO.Path]::GetFullPath($WorkRepo)
if (-not (Test-Path -LiteralPath $repo)) {
    & git clone --filter=blob:none --branch qa/current --single-branch $remoteUrl $repo
    if ($LASTEXITCODE -ne 0) { throw "Creating isolated QA checkout failed" }
}

if (-not (Test-Path -LiteralPath (Join-Path $repo ".git"))) { throw "QA path is not a standalone Git checkout" }
$checkoutRemote = (& git -C $repo remote get-url origin).Trim()
if ($LASTEXITCODE -ne 0 -or $checkoutRemote -ne $remoteUrl) { throw "QA checkout origin differs from source repository" }
$branch = (& git -C $repo branch --show-current).Trim()
if ($branch -ne "qa/current") { throw "QA checkout must be on qa/current; current branch is '$branch'" }
$dirty = @(& git -C $repo status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -gt 0) { throw "QA checkout has local changes; inspect $repo before updating" }

& git -C $repo fetch origin qa/current
if ($LASTEXITCODE -ne 0) { throw "Fetching remote QA failed" }
& git -C $repo merge --ff-only origin/qa/current
if ($LASTEXITCODE -ne 0) { throw "QA checkout cannot fast-forward; local commits were preserved" }
$qaCommit = (& git -C $repo rev-parse HEAD).Trim()
$remoteCommit = (& git -C $repo rev-parse origin/qa/current).Trim()
if ($qaCommit -ne $remoteCommit) { throw "QA checkout does not match remote QA" }
$buildPath = Join-Path $repo "build_qa_oneclick_isolated"

function Assert-PreviewBuild {
    $cachePath = Join-Path $buildPath "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Preview isolation build cache is missing" }
    $cache = Get-Content -LiteralPath $cachePath
    if (-not ($cache -contains "ARIS_PREVIEW_ISOLATION:BOOL=ON")) { throw "Preview isolation is not enabled in the QA build" }
    $rootEntry = @($cache | Where-Object { $_ -match '^ARIS_PREVIEW_ISOLATION_ROOT:PATH=' })
    if ($rootEntry.Count -ne 1) { throw "Preview isolation root is missing from the QA build" }
    $builtRoot = $rootEntry[0].Substring("ARIS_PREVIEW_ISOLATION_ROOT:PATH=".Length)
    if (-not $builtRoot -or [IO.Path]::GetFullPath($builtRoot) -ine $dataRoot) {
        throw "Preview isolation root does not match this QA package"
    }
}

$keyId = $env:STABLE_UPDATE_PACKAGE_KEY_ID
$publicHex = $env:ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX
if (-not $keyId -or -not $publicHex) {
    $keyId = (& gh variable get STABLE_UPDATE_PACKAGE_KEY_ID -R xindong/harken-search).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read STABLE_UPDATE_PACKAGE_KEY_ID" }
    $publicB64 = (& gh variable get UPDATE_PUBLIC_KEY_B64 -R xindong/harken-search).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read UPDATE_PUBLIC_KEY_B64" }
    $publicBytes = [Convert]::FromBase64String($publicB64)
    if ($publicBytes.Length -ne 32) { throw "UPDATE_PUBLIC_KEY_B64 must contain 32 bytes" }
    $publicHex = ([BitConverter]::ToString($publicBytes)).Replace("-", "").ToLowerInvariant()
}

$null = & cmake -S $repo -B $buildPath "-DARIS_PREVIEW_ISOLATION=ON" "-DARIS_PREVIEW_ISOLATION_ROOT=$dataRoot"
if ($LASTEXITCODE -ne 0) { throw "Preview isolation configure failed; EXE was not opened" }
Assert-PreviewBuild
$package = Join-Path $repo "scripts/package_qa_exe.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $package -BuildDir $buildPath -DistDir $dist -FeedbackMode disabled -PackageKeyId $keyId -PackagePublicKeyHex $publicHex
if ($LASTEXITCODE -ne 0) { throw "QA packaging failed; EXE was not opened" }
Assert-PreviewBuild

$exe = Join-Path (Join-Path $dist "Airs") "HarkenSearch.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Packaged QA EXE is missing: $exe" }
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host ">> QA commit: $qaCommit"
Write-Host ">> QA EXE: $exe"
Write-Host ">> SHA256: $exeHash"
if (-not $NoOpen) {
    foreach ($subdir in @("profile/Local", "profile/Roaming", "profile/runtime", "temp")) {
        New-Item -ItemType Directory -Path (Join-Path $dataRoot $subdir) -Force | Out-Null
    }
    $saved = @{}
    foreach ($name in @("LOCALAPPDATA", "APPDATA", "AURA_RUNTIME_ROOT", "TEMP", "TMP")) {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }
    try {
        $env:LOCALAPPDATA = Join-Path $dataRoot "profile/Local"
        $env:APPDATA = Join-Path $dataRoot "profile/Roaming"
        $env:AURA_RUNTIME_ROOT = Join-Path $dataRoot "profile/runtime"
        $env:TEMP = Join-Path $dataRoot "temp"
        $env:TMP = $env:TEMP
        $started = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -PassThru
    }
    finally {
        foreach ($name in $saved.Keys) {
            [Environment]::SetEnvironmentVariable($name, $saved[$name], "Process")
        }
    }
    Start-Sleep -Seconds 4
    $started.Refresh()
    if ($started.HasExited) { throw "QA EXE exited immediately; inspect isolated data at $dataRoot" }
    if ([IO.Path]::GetFullPath($started.Path) -ine [IO.Path]::GetFullPath($exe)) {
        throw "Started process does not match QA EXE"
    }
    Write-Host ">> Opened QA EXE (PID $($started.Id)); isolated data: $dataRoot"
}
