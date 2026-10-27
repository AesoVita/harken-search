[CmdletBinding()]
param(
    [string]$OldPairDir,
    [string]$NewPairDir,
    [string]$PayloadDir,
    [string]$DemoRoot = "",
    [string]$SourceCommit = "",
    [switch]$Cancel,
    [switch]$Serve
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$gitCommonDir = (& git -C $root rev-parse --path-format=absolute --git-common-dir).Trim()
if (-not $gitCommonDir) { throw "Cannot resolve the Git common directory for DemoRoot" }
$mainRepoRoot = Split-Path -Parent $gitCommonDir
if (-not $DemoRoot) {
    $DemoRoot = Join-Path (Split-Path -Parent $mainRepoRoot) "aris-local-update-demo-20260803"
}
$helper = Join-Path $root "tools\prepare_local_update_demo.py"
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "Demo helper is missing" }
$head = (& git -C $root rev-parse HEAD).Trim()
if (-not $SourceCommit) { $SourceCommit = $head }
# The clean provenance gate is intentionally equivalent to: git status --porcelain
$status = (& git -C $root status --porcelain)
if (-not $Cancel -and -not $Serve -and $status) {
    throw "SourceCommit does not match the clean worktree HEAD; demo packaging requires clean provenance"
}
if (-not $env:ARIS_UPDATE_LOCAL_DEMO_PUBLIC_KEY_HEX -or
    -not $env:ARIS_UPDATE_LOCAL_DEMO_KEY_ID) {
    throw "Demo verification identity must be supplied by the process environment"
}
if (-not $Cancel -and -not $Serve -and -not $env:ARIS_UPDATE_LOCAL_DEMO_PRIVATE_KEY_B64) {
    throw "Demo signing identity must be supplied by the process environment"
}

$arguments = @(
    "-B", $helper,
    "--demo-root", $DemoRoot,
    "--mirror-base-url", "http://127.0.0.1:4328/harken-search-demo/",
    "--old-version", "1.0.2",
    "--new-version", "1.0.3",
    "--old-build-id", "local-demo-old-b76dd15d",
    "--new-build-id", "local-demo-new-b76dd15d",
    "--key-id", $env:ARIS_UPDATE_LOCAL_DEMO_KEY_ID,
    "--public-key-hex", $env:ARIS_UPDATE_LOCAL_DEMO_PUBLIC_KEY_HEX,
    "--source-commit", $SourceCommit
)
if ($Cancel) {
    $arguments += "--cancel"
} elseif ($Serve) {
    $arguments += "--serve"
} else {
    if (-not $OldPairDir -or -not $NewPairDir -or -not $PayloadDir) {
        throw "OldPairDir, NewPairDir and PayloadDir are required unless -Cancel or -Serve is used"
    }
    $arguments += @(
        "--old-pair-dir", $OldPairDir,
        "--new-pair-dir", $NewPairDir,
        "--payload-dir", $PayloadDir
    )
}

Push-Location $root
try {
    & python @arguments
    if ($LASTEXITCODE -ne 0) { throw "Local demo preparation failed (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}
