[CmdletBinding()]
param(
    [string]$RepoPath = "",
    [string]$DistDir = "",
    [string]$PackageKeyId = $env:STABLE_UPDATE_PACKAGE_KEY_ID,
    [string]$PackagePublicKeyHex = $env:ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX,
    [switch]$NoOpen
)

$ErrorActionPreference = "Stop"
if (-not $RepoPath) { $RepoPath = Split-Path $PSScriptRoot -Parent }
$repo = (Resolve-Path -LiteralPath $RepoPath).Path
$root = (& git -C $repo rev-parse --show-toplevel).Trim()
if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($root) -ine [IO.Path]::GetFullPath($repo)) {
    throw "RepoPath must be the root of a standalone QA checkout"
}
$branch = (& git -C $repo branch --show-current).Trim()
if ($branch -ne "qa/current") { throw "Local checkout must be on qa/current; current branch is '$branch'" }

if (-not $DistDir) {
    $DistDir = Join-Path "D:/Aris-QA" ((Get-Date -Format "yyyyMMdd") + "-local-qa-current")
}
$dist = [IO.Path]::GetFullPath($DistDir)
$fixedQa = [IO.Path]::GetFullPath((Join-Path $repo "dist_qa"))
if ($dist -ieq $fixedQa -or $dist.StartsWith($fixedQa + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Local package must not overwrite the fixed remote QA package"
}
if ($dist.StartsWith([IO.Path]::GetFullPath($repo) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Local package must be outside the source checkout"
}
$dataRoot = [IO.Path]::GetFullPath((Join-Path $dist "data"))
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

function Get-SourceState {
    $commit = (& git -C $repo rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read local QA commit" }
    $diffLines = @(& git -C $repo diff --binary --no-ext-diff HEAD)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read local QA tracked changes" }
    $statusLines = @(& git -C $repo status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read local QA file status" }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes(($diffLines -join [Environment]::NewLine))
        $diffHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    }
    finally { $sha.Dispose() }
    return [pscustomobject]@{commit=$commit;diffHash=$diffHash;statusLines=$statusLines}
}

$before = Get-SourceState
if (-not $PackageKeyId -or -not $PackagePublicKeyHex) {
    $PackageKeyId = (& gh variable get STABLE_UPDATE_PACKAGE_KEY_ID -R xindong/harken-search).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read STABLE_UPDATE_PACKAGE_KEY_ID" }
    $publicB64 = (& gh variable get UPDATE_PUBLIC_KEY_B64 -R xindong/harken-search).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read UPDATE_PUBLIC_KEY_B64" }
    $publicBytes = [Convert]::FromBase64String($publicB64)
    if ($publicBytes.Length -ne 32) { throw "UPDATE_PUBLIC_KEY_B64 must contain 32 bytes" }
    $PackagePublicKeyHex = ([BitConverter]::ToString($publicBytes)).Replace("-", "").ToLowerInvariant()
}

$null = & cmake -S $repo -B $buildPath "-DARIS_PREVIEW_ISOLATION=ON" "-DARIS_PREVIEW_ISOLATION_ROOT=$dataRoot"
if ($LASTEXITCODE -ne 0) { throw "Preview isolation configure failed; EXE was not opened" }
Assert-PreviewBuild
$package = Join-Path $repo "scripts/package_qa_exe.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $package -BuildDir $buildPath -DistDir $dist -FeedbackMode disabled -PackageKeyId $PackageKeyId -PackagePublicKeyHex $PackagePublicKeyHex
if ($LASTEXITCODE -ne 0) { throw "Local QA packaging failed; EXE was not opened" }
Assert-PreviewBuild

$after = Get-SourceState
if ($before.commit -ne $after.commit -or $before.diffHash -ne $after.diffHash) {
    throw "Source changed during packaging; EXE was not opened"
}
$exe = Join-Path (Join-Path $dist "Airs") "HarkenSearch.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Packaged local QA EXE is missing: $exe" }
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    kind = "local-current-qa-with-uncommitted-files"
    sourceCommit = $before.commit
    trackedDiffSha256 = $before.diffHash
    hasUncommittedChanges = ($before.statusLines.Count -gt 0)
    statusLines = $before.statusLines
    packagePath = $exe
    packageSha256 = $exeHash
    builtAt = [DateTime]::UtcNow.ToString("o")
}
[IO.File]::WriteAllText((Join-Path $dist "local-qa-source.json"), ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
Write-Host ">> Local QA commit: $($before.commit)"
Write-Host ">> Uncommitted files: $($before.statusLines.Count)"
Write-Host ">> Local QA EXE: $exe"
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
    $windowReady = $false
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        Start-Sleep -Seconds 1
        $started.Refresh()
        if ($started.HasExited) { throw "Local QA EXE exited before showing a window; inspect $dataRoot" }
        if ($started.MainWindowHandle -ne [IntPtr]::Zero) {
            $windowReady = $true
            break
        }
    }
    if (-not $windowReady) { throw "Local QA EXE did not show a window within 30 seconds; inspect other running Harken Search instances" }
    if ([IO.Path]::GetFullPath($started.Path) -ine [IO.Path]::GetFullPath($exe)) {
        throw "Started process does not match local QA EXE"
    }
    Write-Host ">> Opened local QA EXE (PID $($started.Id)); isolated data: $dataRoot"
}
