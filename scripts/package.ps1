# Aris packaging script
# Usage: F5 in VS Code, or: powershell -File scripts/package.ps1
# Optional env vars: ARIS_BUILD_DIR, ARIS_DIST_DIR, ARIS_GENERATOR

param(
    [string]$BuildDir   = "build",
    [string]$DistDir    = "dist",
    [string]$Config     = "Release",
    [string]$Generator  = "",
    [bool]$DeployLatest = $true
)
if ($env:ARIS_BUILD_DIR)  { $BuildDir  = $env:ARIS_BUILD_DIR }
if ($env:ARIS_DIST_DIR)   { $DistDir   = $env:ARIS_DIST_DIR }
if ($env:ARIS_GENERATOR)  { $Generator = $env:ARIS_GENERATOR }

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent

Push-Location $Root
try {

# --- Parse version from CMakeLists.txt ---
$versionLine = Select-String -Path "CMakeLists.txt" -Pattern 'project\(HarkenSearch VERSION ([0-9.]+)' | Select-Object -First 1
$Version = if ($versionLine) { $versionLine.Matches[0].Groups[1].Value } else { "0.0.0" }
Write-Host ">> Aris $Version  config=$Config  build=$BuildDir"

$feedbackConfig = @{
    AIRS_CUEVI_FEEDBACK_ENDPOINT = $env:AIRS_CUEVI_FEEDBACK_ENDPOINT
    AIRS_CUEVI_FEEDBACK_KEY      = $env:AIRS_CUEVI_FEEDBACK_KEY
    AIRS_CUEVI_PROJECT_ID        = $env:AIRS_CUEVI_PROJECT_ID
    AIRS_CUEVI_BOARD_ID          = $env:AIRS_CUEVI_BOARD_ID
    AIRS_CUEVI_MILESTONE_ID      = $env:AIRS_CUEVI_MILESTONE_ID
}
foreach ($name in $feedbackConfig.Keys) {
    if ([string]::IsNullOrWhiteSpace($feedbackConfig[$name])) {
        throw "$name is required for a feedback-enabled package"
    }
}
$feedbackCmakeArgs = @()
foreach ($entry in $feedbackConfig.GetEnumerator()) {
    $feedbackCmakeArgs += "-D$($entry.Key)=$($entry.Value)"
}
$feedbackCmakeArgs += "-DAIRS_BUILD_ID=$Version"
$feedbackCmakeArgs += "-DAIRS_RELEASE_CHANNEL=$Config"

# --- CMake configure ---
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
Write-Host ">> cmake configure..."
$genArgs = if ($Generator) { @("-G", $Generator) } else { @() }
& cmake -B $BuildDir -DCMAKE_BUILD_TYPE=$Config @genArgs @feedbackCmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

# --- CMake build ---
Write-Host ">> cmake build --target HarkenSearch..."
& cmake --build $BuildDir --config $Config --target HarkenSearch --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }

# --- Locate exe ---
# VS multi-config generators put output under build/Release/; single-config under build/
$exeRelease = Join-Path $BuildDir "$Config\HarkenSearch.exe"
$exeFlat    = Join-Path $BuildDir "HarkenSearch.exe"
$exePath    = if (Test-Path $exeRelease) { $exeRelease } elseif (Test-Path $exeFlat) { $exeFlat } else {
    throw "HarkenSearch.exe not found under $BuildDir"
}
$BinDir = Split-Path $exePath -Parent
Write-Host ">> exe: $exePath"

# --- Assemble dist ---
$PackageName = "Aris-$Version-win64"
$PackageDir  = Join-Path $DistDir $PackageName
if (Test-Path $PackageDir) { Remove-Item $PackageDir -Recurse -Force }
New-Item $PackageDir -ItemType Directory | Out-Null

# Copy exe + all DLLs + known asset files next to exe
Get-ChildItem $BinDir -File | Where-Object {
    $_.Extension -in @(".exe", ".dll", ".sig") -or $_.Name -match "\.(png|ttf|otf|json|lottie)$"
} | Copy-Item -Destination $PackageDir

# Copy assets subdirectories if present (icons, models, etc.)
foreach ($subdir in @("icons", "models", "assets")) {
    $src = Join-Path $BinDir $subdir
    if (Test-Path $src) {
        Copy-Item $src -Destination $PackageDir -Recurse -Force
    }
}

# 将与当前正式图解一致的离线 PDF 放入便携包。
& python -B (Join-Path $Root "tools/build_offline_manual.py") --stage-dir $PackageDir
if ($LASTEXITCODE -ne 0) { throw "Offline manual staging failed (exit $LASTEXITCODE)" }

# --- Create ZIP ---
$ZipPath = Join-Path $DistDir "$PackageName.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $PackageDir -DestinationPath $ZipPath
Write-Host ">> package: $ZipPath"

# --- Refresh the local latest build ---
# Only close the exact package we replace. Development worktrees and any other
# Aris process stay untouched, even when they share the same executable name.
if ($DeployLatest) {
    $LatestArisExe = Join-Path $Root "dist_latest\Airs\HarkenSearch.exe"
    $LatestArisDir = Split-Path $LatestArisExe -Parent
    $LatestArisSig = "$LatestArisExe.sig"
    $SourceArisSig = "$exePath.sig"
    if (-not (Test-Path $SourceArisSig)) {
        throw "HarkenSearch.exe signature not found beside $exePath"
    }

    $LatestArisFullPath = [IO.Path]::GetFullPath($LatestArisExe)
    $runningLatest = @(Get-CimInstance Win32_Process -Filter "Name='HarkenSearch.exe'" -ErrorAction SilentlyContinue |
        Where-Object {
            $_.ExecutablePath -and
            ([IO.Path]::GetFullPath($_.ExecutablePath) -ieq $LatestArisFullPath)
        })
    foreach ($process in $runningLatest) {
        Stop-Process -Id $process.ProcessId -Force
    }

    # A terminated Windows process may keep its executable mapped briefly.
    # Do not copy or restart until the exact target process is gone and the
    # target file can be opened exclusively.
    $LatestArisUnlocked = $false
    for ($attempt = 0; $attempt -lt 50; $attempt++) {
        $stillRunningLatest = @(Get-CimInstance Win32_Process -Filter "Name='HarkenSearch.exe'" -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ExecutablePath -and
                ([IO.Path]::GetFullPath($_.ExecutablePath) -ieq $LatestArisFullPath)
            })
        if ($stillRunningLatest.Count -eq 0) {
            if (-not (Test-Path $LatestArisExe)) {
                $LatestArisUnlocked = $true
                break
            }
            try {
                $lockProbe = [IO.File]::Open($LatestArisExe, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
                $lockProbe.Dispose()
                $LatestArisUnlocked = $true
                break
            } catch {
                # The process is gone but Windows has not released the image yet.
            }
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $LatestArisUnlocked) {
        throw "Latest HarkenSearch.exe did not unlock after the verified process stopped"
    }

    New-Item -ItemType Directory -Path $LatestArisDir -Force | Out-Null
    Copy-Item $exePath -Destination $LatestArisExe -Force
    Copy-Item $SourceArisSig -Destination $LatestArisSig -Force

    $SourceArisHash = (Get-FileHash $exePath -Algorithm SHA256).Hash
    $LatestArisHash = (Get-FileHash $LatestArisExe -Algorithm SHA256).Hash
    if ($SourceArisHash -ne $LatestArisHash) {
        throw "Deployed HarkenSearch.exe hash does not match the package artifact"
    }
    $SourceArisSigHash = (Get-FileHash $SourceArisSig -Algorithm SHA256).Hash
    $LatestArisSigHash = (Get-FileHash $LatestArisSig -Algorithm SHA256).Hash
    if ($SourceArisSigHash -ne $LatestArisSigHash) {
        throw "Deployed HarkenSearch.exe.sig hash does not match the package signature"
    }

    Start-Process -FilePath $LatestArisExe -WorkingDirectory $LatestArisDir
    Write-Host ">> refreshed and started: $LatestArisExe"
}

} finally {
    Pop-Location
}
