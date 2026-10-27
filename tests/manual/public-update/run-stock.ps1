$ErrorActionPreference = 'Stop'
$out = 'C:/qa-output'
if ($env:USERNAME -ne 'WDAGUtilityAccount') { throw 'Sandbox required' }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$runId = [Guid]::NewGuid().ToString('N')
$testExit = 1
$verificationPath = Join-Path $out 'applied-independent-verification.json'
@{ runId = $runId; passed = $false; status = 'running'; observedUtc = [DateTime]::UtcNow.ToString('o') } |
  ConvertTo-Json | Set-Content -LiteralPath $verificationPath -Encoding UTF8
Start-Process powershell.exe -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/qa-input/guest-ui.ps1' -WindowStyle Hidden | Out-Null
$install = 'C:/Program Files/Harken Search'
$exe = Join-Path $install 'HarkenSearch.exe'
$log = Join-Path $out 'result.log'
$expected = Get-Content -LiteralPath 'C:/qa-input/expected.json' -Raw -Encoding UTF8 | ConvertFrom-Json
$updates = Join-Path $env:LOCALAPPDATA 'HarkenSearch/updates'

function Log([string]$message) { Add-Content -LiteralPath $log -Value ((Get-Date -Format o) + ' ' + $message) -Encoding UTF8 }
function Shot([string]$name) {
  Add-Type -AssemblyName System.Windows.Forms,System.Drawing
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object Drawing.Bitmap($b.Width, $b.Height)
  $g = [Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
  $bmp.Save((Join-Path $out $name), [Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}
# Process-only probe. During the update transaction this script must not open the
# installed EXE or enumerate the updates tree: those handles compete with the
# bootstrap's own root opens and journal writes.
function ProcessState {
  $apps = @(Get-Process HarkenSearch -ErrorAction SilentlyContinue)
  $bootstrap = @(Get-Process HarkenSearchBootstrap -ErrorAction SilentlyContinue)
  [pscustomobject]@{ appPids = @($apps | ForEach-Object { $_.Id }); bootstrapPids = @($bootstrap | ForEach-Object { $_.Id }) }
}
# One-shot filesystem verification, taken only after the transaction has settled.
function FileEvidence {
  $item = Get-Item -LiteralPath $exe -ErrorAction SilentlyContinue
  $sig = if ($item) { Get-AuthenticodeSignature -LiteralPath $exe } else { $null }
  $roots = @(Get-ChildItem -LiteralPath $updates -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'transaction-*' -or $_.Name -like 'staging-invocation-*' })
  $residue = @($roots | ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue })
  $failure = Join-Path $updates 'bootstrap-last-failure.txt'
  [pscustomobject]@{
    sha = if ($item) { (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    version = if ($item) { $item.VersionInfo.FileVersion } else { $null }
    signature = if ($sig) { [string]$sig.Status } else { 'None' }
    signer = if ($sig -and $sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { '' }
    roots = @($roots.FullName); residue = $residue.Count
    failurePresent = Test-Path -LiteralPath $failure
    failureText = if (Test-Path -LiteralPath $failure) { (Get-Content -LiteralPath $failure -Raw) -replace "`r|`n", ' ' } else { '' }
  }
}

$oldPid = 0
try {
  New-Item -ItemType Directory -Force -Path $out, $install | Out-Null
  'isolated_public_upgrade_1038_to_1039_passive_watch' | Set-Content -LiteralPath $log -Encoding ASCII
  Log ('USER=' + [Security.Principal.WindowsIdentity]::GetCurrent().Name)

  $regKeys = @(
    'HKLM:/SOFTWARE/WOW6432Node/Microsoft/EdgeUpdate/Clients/{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKLM:/SOFTWARE/Microsoft/EdgeUpdate/Clients/{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKCU:/SOFTWARE/Microsoft/EdgeUpdate/Clients/{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
  )
  function WebViewRuntime {
    foreach ($key in $regKeys) {
      $prop = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
      if ($prop -and $prop.pv -and $prop.location) {
        $file = Join-Path $prop.location ($prop.pv + '/msedgewebview2.exe')
        if (Test-Path -LiteralPath $file) { return [pscustomobject]@{ pv = $prop.pv; file = $file } }
      }
    }
    return $null
  }
  $runtime = WebViewRuntime
  if ($null -eq $runtime) {
    $webview = Get-ChildItem -LiteralPath 'C:/qa-prerequisite' -Filter '*WebView2*X64*exe' -File | Select-Object -First 1
    if ($null -eq $webview) { throw 'WebView2 offline prerequisite is missing' }
    if ((Get-FileHash -LiteralPath $webview.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne '0625fb5c1e75be58a164e14f95aa58a3e5badb859b3d17c952385f3e4a9ff3e8') { throw 'WebView2 prerequisite hash mismatch' }
    $dep = Start-Process -FilePath $webview.FullName -ArgumentList '/silent', '/install' -Verb RunAs -WindowStyle Hidden -PassThru
    Log ('WEBVIEW_INSTALL_WAIT=' + $dep.WaitForExit(300000))
    for ($i = 0; $i -lt 60; $i++) { $runtime = WebViewRuntime; if ($runtime) { break }; Start-Sleep -Seconds 2 }
  }
  if ($null -eq $runtime) { throw 'No usable WebView2 runtime is registered' }
  $runtimeSignature = Get-AuthenticodeSignature -LiteralPath $runtime.file
  Log ('WEBVIEW_VERSION=' + $runtime.pv + '|SIGNATURE=' + $runtimeSignature.Status)
  if ($runtimeSignature.Status -ne 'Valid' -or $runtimeSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'WebView2 runtime signature invalid' }

  $vc = 'C:/qa-input/vc_redist.x64.exe'
  if (-not (Test-Path -LiteralPath $vc)) { throw 'VC runtime offline package is missing' }
  if ((Get-FileHash -LiteralPath $vc -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b') { throw 'VC runtime package hash mismatch' }
  if (-not (Test-Path -LiteralPath 'C:/Windows/System32/MSVCP140.dll')) {
    $vcProcess = Start-Process -FilePath $vc -ArgumentList '/install', '/quiet', '/norestart', '/log', (Join-Path $out 'vc-install.log') -WindowStyle Hidden -PassThru
    Log ('VC_INSTALL_WAIT=' + $vcProcess.WaitForExit(600000))
    for ($i = 0; $i -lt 120; $i++) { if (Test-Path -LiteralPath 'C:/Windows/System32/MSVCP140.dll') { break }; Start-Sleep -Seconds 3 }
  }
  if (-not (Test-Path -LiteralPath 'C:/Windows/System32/MSVCP140.dll')) { throw 'MSVCP140.dll still missing after VC runtime install' }
  Log 'VC_RUNTIME_READY'

  $response = Invoke-WebRequest -UseBasicParsing -Uri $expected.manifestUrl -TimeoutSec 45
  $manifest = $response.Content | ConvertFrom-Json
  $response.Content | Set-Content -LiteralPath (Join-Path $out 'public-manifest-before.json') -Encoding UTF8
  Log ('PUBLIC_MANIFEST_HTTP=' + $response.StatusCode + '|VERSION=' + $manifest.version + '|BUILD=' + $manifest.build_id)
  if ($manifest.version -ne $expected.newVersion -or $manifest.build_id -ne $expected.mainCommit) { throw 'Public manifest does not advertise 1.0.39 from main' }

  if (Test-Path -LiteralPath 'C:/qa-seed/docs/manual') { throw 'Original seed must not contain the newly introduced manual directory' }
  Copy-Item -Path 'C:/qa-seed/*' -Destination $install -Recurse -Force
  $seedSha = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
  $seedSignature = Get-AuthenticodeSignature -LiteralPath $exe
  Log ('OLD_EXE_SHA=' + $seedSha + '|VERSION=' + (Get-Item -LiteralPath $exe).VersionInfo.FileVersion + '|SIGN=' + $seedSignature.Status)
  if ($seedSha -ne $expected.oldExeSha256 -or (Get-Item -LiteralPath $exe).VersionInfo.FileVersion -ne $expected.oldVersion -or [string]$seedSignature.Status -ne 'Valid') { throw 'Seed is not the published stock 1.0.38 binary' }
  [pscustomobject]@{ observedUtc = [DateTime]::UtcNow.ToString('o'); role = 'old client'; version = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion;
                     exeSha256 = $seedSha; authenticode = [string]$seedSignature.Status; signer = $seedSignature.SignerCertificate.Subject;
                     payloadSource = $expected.seedSource } |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'install-identity.json') -Encoding UTF8

  $regParent = 'HKLM:/SOFTWARE/Microsoft/Windows/CurrentVersion/Uninstall'
  $regName = '{31B6E215-3D2B-43EA-88C8-72D73ABC9A0C}}_is1'
  $regPath = Join-Path $regParent $regName
  if (Test-Path -LiteralPath $regPath) { throw 'Install registration is not fresh' }
  New-Item -Path $regParent -Name $regName -Force | Out-Null
  New-ItemProperty -LiteralPath $regPath -Name InstallLocation -Value $install -PropertyType String -Force | Out-Null
  New-ItemProperty -LiteralPath $regPath -Name DisplayVersion -Value $expected.oldVersion -PropertyType String -Force | Out-Null

  $consentRoot = Join-Path $env:LOCALAPPDATA 'HarkenSearch'
  New-Item -ItemType Directory -Force -Path $consentRoot | Out-Null
  @('# Disposable sandbox legal-consent fixture', 'eulaVersion=1', 'privacyVersion=1',
    ('acceptedAtUtc=' + [DateTime]::UtcNow.ToString('o')), ('appVersion=' + $expected.oldVersion)) |
    Set-Content -LiteralPath (Join-Path $consentRoot 'settings.ini.legal-consent') -Encoding ASCII

  $p = Start-Process -FilePath $exe -WorkingDirectory $install -PassThru
  $oldPid = $p.Id
  Log ('OLD_APP_PID=' + $oldPid)

  # Download observation: reading the staged package is safe here because no
  # update transaction exists yet.
  $zipPath = $null; $zipHash = $null; $zipBytes = 0; $downloadPassed = $false
  for ($i = 0; $i -lt 300; $i++) {
    Start-Sleep -Seconds 3
    $zips = @(Get-ChildItem -LiteralPath $updates -Filter '*.zip' -File -Recurse -Force -ErrorAction SilentlyContinue)
    foreach ($candidate in $zips) {
      $candidateHash = (Get-FileHash -LiteralPath $candidate.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
      if ($candidateHash -eq $expected.newPayloadSha256) {
        $zipPath = $candidate.FullName; $zipHash = $candidateHash; $zipBytes = $candidate.Length
        $downloadPassed = ($candidate.Length -eq $manifest.asset.size)
        Log ('STAGED_ZIP=' + $candidate.FullName + '|SHA=' + $candidateHash + '|BYTES=' + $candidate.Length + '|MATCH=' + $downloadPassed)
        break
      }
    }
    if ($downloadPassed) { break }
    if ($i % 20 -eq 19) { Log ('DOWNLOAD_WAIT=' + $i + '|APPS=' + ((ProcessState).appPids -join ',') + '|ZIPS=' + $zips.Count) }
  }
  Shot 'update-ready-before-restart.png'
  [pscustomobject]@{
    runId = $runId
    observedUtc = [DateTime]::UtcNow.ToString('o'); sandbox = $env:COMPUTERNAME
    oldVersion = $expected.oldVersion; oldExeSha256 = $expected.oldExeSha256; oldPid = $oldPid
    manifestUrl = $expected.manifestUrl; manifestVersion = $manifest.version; manifestBuild = $manifest.build_id
    stagedZip = $zipPath; stagedZipSha256 = $zipHash; stagedZipBytes = $zipBytes
    expectedZipSha256 = $expected.newPayloadSha256
    automaticPublicDownloadPassed = $downloadPassed
    restartConfirmation = 'waiting for explicit test confirmation on the real update dialog'
    watcherMode = 'process-only while the transaction is open'
    cliCompiledIn = $false; uiAutomationInvokeAvailable = $false
    limitation = 'Public stock build has no update CLI and ImGui exposes no actionable UIAutomation element; restart confirmation is a real UI click by the test operator inside the disposable sandbox; original published binary and original directory structure are unchanged.'
  } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'download-acceptance.json') -Encoding UTF8
  if (-not $downloadPassed) { throw 'Automatic public download of 1.0.39 was not observed' }
  Log 'PUBLIC_DOWNLOAD_READY'

  # From here on: process handles only, until the transaction settles.
  $newPidSeen = @()
  $settled = ''
  $bootstrapWasSeen = $false
  for ($i = 0; $i -lt 300; $i++) {
    Start-Sleep -Seconds 2
    if (Test-Path -LiteralPath (Join-Path $updates 'bootstrap-last-failure.txt')) { $settled = 'bootstrap-reported-failure'; break }
    $state = ProcessState
    if ($state.appPids -notcontains $oldPid -and $state.appPids.Count -eq 0 -and $state.bootstrapPids.Count -eq 0 -and $i -gt 5) {
      # nothing running and nothing replaced: give the bootstrap a moment, then settle
    }
    if ($state.bootstrapPids.Count -gt 0) { $bootstrapWasSeen = $true }
    $newApps = @($state.appPids | Where-Object { $_ -ne $oldPid })
    if ($newApps.Count -gt 0) { $newPidSeen = $newApps }
    if ($i % 15 -eq 14) { Log ('WAIT=' + $i + '|APPS=' + ($state.appPids -join ',') + '|BOOTSTRAP=' + ($state.bootstrapPids -join ',') + '|OLD_ALIVE=' + [bool](Get-Process -Id $oldPid -ErrorAction SilentlyContinue)) }
    if ($newApps.Count -gt 0 -and -not (Get-Process -Id $oldPid -ErrorAction SilentlyContinue) -and $state.bootstrapPids.Count -eq 0) {
      Start-Sleep -Seconds 12
      $after = ProcessState
      if (@($after.appPids | Where-Object { $_ -ne $oldPid }).Count -gt 0 -and $after.bootstrapPids.Count -eq 0) { $settled = 'restarted'; break }
    }
    if ($bootstrapWasSeen -and $state.bootstrapPids.Count -eq 0 -and $newPidSeen.Count -eq 0 -and -not (Get-Process -Id $oldPid -ErrorAction SilentlyContinue)) {
      Start-Sleep -Seconds 8
      $after = ProcessState
      if ($after.bootstrapPids.Count -eq 0 -and @($after.appPids | Where-Object { $_ -ne $oldPid }).Count -eq 0) { $settled = 'bootstrap-exited-without-new-app'; break }
    }
  }
  Log ('SETTLED=' + $settled)

  $final = FileEvidence
  Shot 'after-upgrade.png'
  $passed = ($final.sha -eq $expected.newExeSha256 -and $final.version -eq $expected.newVersion `
             -and $final.signature -eq 'Valid' -and $final.signer -match 'X\.D\. Network Inc\.' `
             -and $newPidSeen.Count -gt 0 -and $settled -eq 'restarted' `
             -and $final.residue -eq 0 -and -not $final.failurePresent)
  [pscustomobject]@{
    runId = $runId
    observedUtc = [DateTime]::UtcNow.ToString('o'); sandbox = $env:COMPUTERNAME
    oldVersion = $expected.oldVersion; newVersion = $expected.newVersion; oldPid = $oldPid; newPids = @($newPidSeen)
    settled = $settled
    expectedNewExeSha256 = $expected.newExeSha256; currentExeSha256 = $final.sha; currentFileVersion = $final.version
    authenticodeStatus = $final.signature; signer = $final.signer
    oldProcessStillRunning = [bool](Get-Process -Id $oldPid -ErrorAction SilentlyContinue)
    transactionRoots = @($final.roots); residueFiles = $final.residue
    failureFilePresent = $final.failurePresent; failureText = $final.failureText
    passed = $passed
    method = 'Process-only observation while the update transaction was open; one filesystem verification pass after it settled. Fresh independent SHA256 of the installed EXE; no product mutation; the restart confirmation is a real test-operator click on the actual dialog inside the disposable sandbox.'
  } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $out 'applied-independent-verification.json') -Encoding UTF8
  Log ('APPLIED_PASSED=' + $passed)
  if (-not $passed) { throw ('upgrade acceptance failed: ' + $final.failureText) }
  Log 'UPGRADE_1038_TO_1039_PASSED'
  $testExit = 0
} catch {
  Log ('TEST_FAILED=' + $_.Exception.Message)
  $failureReport = Get-Content -LiteralPath $verificationPath -Raw -Encoding UTF8 | ConvertFrom-Json
  $failureReport.passed = $false
  $failureReport | Add-Member -NotePropertyName error -NotePropertyValue $_.Exception.Message -Force
  $failureReport | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $verificationPath -Encoding UTF8
} finally {
  if (Test-Path -LiteralPath $updates) {
    Get-ChildItem -LiteralPath $updates -Force -Recurse -ErrorAction SilentlyContinue |
      Select-Object FullName, Length, LastWriteTime | ConvertTo-Json -Depth 5 |
      Set-Content -LiteralPath (Join-Path $out 'final-update-tree.json') -Encoding UTF8
    $patch = Join-Path $updates 'patch-info.json'
    if (Test-Path -LiteralPath $patch) { Copy-Item -LiteralPath $patch -Destination (Join-Path $out 'patch-info.json') -Force }
    $failure = Join-Path $updates 'bootstrap-last-failure.txt'
    if (Test-Path -LiteralPath $failure) { Copy-Item -LiteralPath $failure -Destination (Join-Path $out 'bootstrap-last-failure.txt') -Force }
  }
  Get-CimInstance Win32_Process -Filter "Name='HarkenSearch.exe' or Name='HarkenSearchBootstrap.exe'" -ErrorAction SilentlyContinue |
    Select-Object ProcessId, ParentProcessId, Name, CommandLine | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $out 'processes.json') -Encoding UTF8
}
exit $testExit
