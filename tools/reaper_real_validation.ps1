param(
    [string]$ReaperRoot = 'E:\Reaper',
    [string]$CandidateDll = '.local\reaper-goal-20260713\build_public\Release\reaper_harken.dll',
    [string]$BridgeHarness = 'build_dbg\Debug\TestReaperBridgeContract.exe',
    [string]$OutputRoot = '.local\reaper-goal-20260713\real_reaper',
    [switch]$IsolatedHost
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

function Resolve-RequiredPath([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label 不存在：$Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Wait-ForFile([string]$Path, [int]$TimeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $Path) { return }
        Start-Sleep -Milliseconds 100
    }
    throw "等待 REAPER 验收信号超时：$Path"
}

function New-TestWave([string]$Path, [double]$Seconds, [double]$Frequency) {
    $sampleRate = 48000
    $sampleCount = [int]($sampleRate * $Seconds)
    $dataBytes = $sampleCount * 2
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        $writer = [IO.BinaryWriter]::new($stream)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([int](36 + $dataBytes))
        $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $writer.Write([int]16)
        $writer.Write([int16]1)
        $writer.Write([int16]1)
        $writer.Write([int]$sampleRate)
        $writer.Write([int]($sampleRate * 2))
        $writer.Write([int16]2)
        $writer.Write([int16]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([int]$dataBytes)
        for ($index = 0; $index -lt $sampleCount; ++$index) {
            $sample = [Math]::Sin(2.0 * [Math]::PI * $Frequency * $index / $sampleRate)
            $writer.Write([int16]($sample * 4096.0))
        }
        $writer.Flush()
    } finally {
        $stream.Dispose()
    }
}

if (-not $IsolatedHost -and (Get-Process reaper -ErrorAction SilentlyContinue)) {
    throw 'REAPER 正在运行；真实验收拒绝覆盖扩展或触碰当前工程。请等待宿主自然退出后重试。'
}

$reaperExe = Resolve-RequiredPath (Join-Path $ReaperRoot 'reaper.exe') 'REAPER 主程序'
$candidate = Resolve-RequiredPath $CandidateDll '候选 REAPER 扩展'
$harness = Resolve-RequiredPath $BridgeHarness 'REAPER 验收客户端'
$outputBase = if ([IO.Path]::IsPathRooted($OutputRoot)) {
    [IO.Path]::GetFullPath($OutputRoot)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputRoot))
}
[IO.Directory]::CreateDirectory($outputBase) | Out-Null
$runRoot = Join-Path $outputBase ((Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') + '-' + $PID)
$assets = Join-Path $runRoot '中文 素材（验收）'
[IO.Directory]::CreateDirectory($assets) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

$single = Join-Path $assets '单个 提示音.wav'
$batchFirst = Join-Path $assets '第一条 hit.wav'
$batchSecond = Join-Path $assets '第二条（尾音）#.wav'
$invalid = Join-Path $assets '损坏 文件.wav'
New-TestWave $single 0.35 440.0
New-TestWave $batchFirst 0.40 550.0
New-TestWave $batchSecond 0.55 660.0
[IO.File]::WriteAllBytes($invalid, [byte[]]::new(0))

$alternateConfig = ''
$pipeName = '\\.\pipe\ArisReaperBridge'
if ($IsolatedHost) {
    $isolatedResource = Join-Path $runRoot 'reaper-portable-resource'
    $isolatedPlugins = Join-Path $isolatedResource 'UserPlugins'
    [IO.Directory]::CreateDirectory($isolatedPlugins) | Out-Null
    $alternateConfig = Join-Path $isolatedResource 'reaper.ini'
    [IO.File]::WriteAllText(
        $alternateConfig,
        "[REAPER]`r`nnewprojdo=0`r`nvstpath64=`r`nvstfullstate=0`r`n",
        $utf8)
    [IO.File]::WriteAllText((Join-Path $isolatedResource 'reaper-vstplugins64.ini'), '', $utf8)
    [IO.File]::WriteAllText((Join-Path $isolatedResource 'reaper-vstshells64.ini'), '', $utf8)
    $installedDll = Join-Path $isolatedPlugins 'reaper_harken.dll'
    Copy-Item -LiteralPath $candidate -Destination $installedDll
    $pipeName = "\\.\pipe\ArisReaperBridgeReal_$PID"
    $env:ARIS_REAPER_BRIDGE_PIPE_NAME = $pipeName
} else {
    & $harness --doctor-update $ReaperRoot $candidate
    if ($LASTEXITCODE -ne 0) { throw "Doctor 更新失败，退出码：$LASTEXITCODE" }
    $installedDll = Join-Path $ReaperRoot 'UserPlugins\reaper_harken.dll'
}
$candidateHash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
$installedHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
if ($candidateHash -ne $installedHash) { throw '真实 UserPlugins 中的扩展哈希与候选文件不一致。' }

$projectPath = Join-Path $runRoot 'validation.rpp'
$luaPath = Join-Path $runRoot 'validation.lua'
$evidencePath = Join-Path $runRoot 'timeline.tsv'
$completePath = Join-Path $runRoot 'complete.signal'
$failureDonePath = Join-Path $runRoot 'failure.done'
$rpp = @'
<REAPER_PROJECT 0.1 "7.54/win64" 0
  RIPPLE 0
  GROUPOVERRIDE 0 0 0
  AUTOXFADE 1
  ENVATTACH 1
  POOLEDENVATTACH 0
  MIXERUIFLAGS 11 48
  PEAKGAIN 1
  FEEDBACK 0
  PANLAW 1
  PROJOFFS 0 0 0
  MAXPROJLEN 0 0
  GRID 3199 8 1 8 1 0 0 0
  TIMEMODE 1 5 -1 30 0 0 -1
  VIDEO_CONFIG 0 0 256
  PANMODE 3
  CURSOR 30
  ZOOM 100 0 0
  VZOOMEX 6 0
  USE_REC_CFG 0
  RECMODE 1
  SMPTESYNC 0 30 100 40 1000 300 0 0 1 0 0
  LOOP 0
  SAMPLERATE 48000 0 0
>
'@
[IO.File]::WriteAllText($projectPath, $rpp, $utf8)

$luaRoot = $runRoot.Replace('\', '\\')
$lua = @'
local root = "__ROOT__"
local project_path = root .. "\\validation.rpp"
local evidence_path = root .. "\\timeline.tsv"
local failure_done = root .. "\\failure.done"
local deadline = reaper.time_precise() + 120.0
local phase = 0

local function write_file(path, text)
  local file = assert(io.open(path, "wb"))
  file:write(text)
  file:close()
end

local function append_line(text)
  local file = assert(io.open(evidence_path, "ab"))
  file:write(text .. "\n")
  file:close()
end

local function file_exists(path)
  local file = io.open(path, "rb")
  if not file then return false end
  file:close()
  return true
end

local function snapshot(name)
  reaper.Main_SaveProject(0, false)
  local source = assert(io.open(project_path, "rb"))
  local bytes = source:read("*a")
  source:close()
  write_file(root .. "\\" .. name .. ".rpp", bytes)
end

local function spans()
  local result = {}
  for index = 0, reaper.CountMediaItems(0) - 1 do
    local item = reaper.GetMediaItem(0, index)
    local start_pos = reaper.GetMediaItemInfo_Value(item, "D_POSITION")
    local length = reaper.GetMediaItemInfo_Value(item, "D_LENGTH")
    result[#result + 1] = { start_pos, start_pos + length }
  end
  table.sort(result, function(a, b) return a[1] < b[1] end)
  return result
end

local function record(name)
  local values = { name, tostring(reaper.CountMediaItems(0)),
                   string.format("%.9f", reaper.GetCursorPosition()) }
  for _, span in ipairs(spans()) do
    values[#values + 1] = string.format("%.9f", span[1])
    values[#values + 1] = string.format("%.9f", span[2])
  end
  append_line(table.concat(values, "\t"))
end

local function clear_project()
  for index = reaper.CountMediaItems(0) - 1, 0, -1 do
    local item = reaper.GetMediaItem(0, index)
    local track = reaper.GetMediaItemTrack(item)
    reaper.DeleteTrackMediaItem(track, item)
  end
  for index = reaper.CountTracks(0) - 1, 0, -1 do
    reaper.DeleteTrack(reaper.GetTrack(0, index))
  end
  reaper.SetEditCurPos(30.0, false, false)
  reaper.UpdateArrange()
end

local function finish_with_error(message)
  append_line("error\t" .. message)
  write_file(root .. "\\error.signal", message)
  reaper.Main_SaveProject(0, false)
  reaper.Main_OnCommand(40004, 0)
end

local function tick()
  if reaper.time_precise() > deadline then
    finish_with_error("timeout")
    return
  end
  if phase == 0 then
    clear_project()
    write_file(root .. "\\ready-single.signal", "ready")
    phase = 1
  elseif phase == 1 and reaper.CountMediaItems(0) == 1 then
    record("single")
    snapshot("single")
    clear_project()
    write_file(root .. "\\ready-batch.signal", "ready")
    phase = 2
  elseif phase == 2 and reaper.CountMediaItems(0) == 2 then
    record("batch")
    snapshot("batch")
    clear_project()
    write_file(root .. "\\ready-failure.signal", "ready")
    phase = 3
  elseif phase == 3 and file_exists(failure_done) then
    record("failure")
    snapshot("failure")
    write_file(root .. "\\complete.signal", "complete")
    reaper.Main_OnCommand(40004, 0)
    return
  end
  reaper.defer(tick)
end

tick()
'@
$lua = $lua.Replace('__ROOT__', $luaRoot)
[IO.File]::WriteAllText($luaPath, $lua, $utf8)

if (-not $IsolatedHost -and (Get-Process reaper -ErrorAction SilentlyContinue)) {
    throw 'Doctor 完成后检测到新的 REAPER 实例；拒绝启动验收工程，请稍后重试。'
}
$reaperArguments = @()
if ($IsolatedHost) {
    $reaperArguments += @('-cfgfile', ('"' + $alternateConfig + '"'))
}
$reaperArguments += @('-newinst', '-nosplash', ('"' + $projectPath + '"'), ('"' + $luaPath + '"'))
$process = if ($IsolatedHost) {
    Start-Process -FilePath $reaperExe -ArgumentList $reaperArguments -PassThru -WindowStyle Minimized
} else {
    Start-Process -FilePath $reaperExe -ArgumentList $reaperArguments -PassThru
}
try {
    Wait-ForFile (Join-Path $runRoot 'ready-single.signal') 90
    $singleOutput = if ($IsolatedHost) {
        (& $harness --send-pipe $pipeName $single 2>&1 | Out-String).Trim()
    } else {
        (& $harness --send $single 2>&1 | Out-String).Trim()
    }
    if ($LASTEXITCODE -ne 0) { throw "单文件发送失败：$singleOutput" }

    Wait-ForFile (Join-Path $runRoot 'ready-batch.signal') 30
    $batchOutput = if ($IsolatedHost) {
        (& $harness --send-pipe $pipeName $batchFirst $batchSecond 2>&1 | Out-String).Trim()
    } else {
        (& $harness --send $batchFirst $batchSecond 2>&1 | Out-String).Trim()
    }
    if ($LASTEXITCODE -ne 0) { throw "批量发送失败：$batchOutput" }

    Wait-ForFile (Join-Path $runRoot 'ready-failure.signal') 30
    $failureOutput = if ($IsolatedHost) {
        (& $harness --send-pipe $pipeName $single $invalid 2>&1 | Out-String).Trim()
    } else {
        (& $harness --send $single $invalid 2>&1 | Out-String).Trim()
    }
    $failureExitCode = $LASTEXITCODE
    [IO.File]::WriteAllText($failureDonePath, 'done', $utf8)
    Wait-ForFile $completePath 30

    if (-not $process.WaitForExit(30000)) {
        $closeArguments = if ($IsolatedHost) {
            @('-cfgfile', ('"' + $alternateConfig + '"'), '-nonewinst', '-close:save:exit')
        } else {
            @('-close:save:exit')
        }
        Start-Process -FilePath $reaperExe -ArgumentList $closeArguments -Wait | Out-Null
        if (-not $process.WaitForExit(30000)) {
            throw 'REAPER 未响应正常退出请求；脚本不会强制结束进程。'
        }
    }
} catch {
    $closeArguments = if ($IsolatedHost) {
        @('-cfgfile', ('"' + $alternateConfig + '"'), '-nonewinst', '-close:save:exit')
    } else {
        @('-close:save:exit')
    }
    Start-Process -FilePath $reaperExe -ArgumentList $closeArguments -Wait | Out-Null
    throw
}

$rows = @{}
foreach ($line in Get-Content -LiteralPath $evidencePath -Encoding UTF8) {
    $parts = $line -split "`t"
    $rows[$parts[0]] = $parts
}
if (-not $rows.ContainsKey('single') -or -not $rows.ContainsKey('batch') -or
    -not $rows.ContainsKey('failure')) {
    throw '真实 REAPER 没有生成完整的三阶段时间线证据。'
}

$culture = [Globalization.CultureInfo]::InvariantCulture
function Number([string]$Value) { return [double]::Parse($Value, $culture) }
$singleRow = $rows['single']
$batchRow = $rows['batch']
$failureRow = $rows['failure']
$singlePass = [int]$singleRow[1] -eq 1 -and (Number $singleRow[3]) -ge 30.0
$batchPass = [int]$batchRow[1] -eq 2 -and
    (Number $batchRow[3]) -ge 30.0 -and (Number $batchRow[5]) -ge (Number $batchRow[4])
$failurePass = $failureExitCode -ne 0 -and [int]$failureRow[1] -eq 0 -and
    [Math]::Abs((Number $failureRow[2]) - 30.0) -lt 0.000001

$batchRppText = [IO.File]::ReadAllText((Join-Path $runRoot 'batch.rpp'))
$positionMatches = [regex]::Matches($batchRppText, '(?m)^\s*POSITION\s+([-+0-9.eE]+)')
$lengthMatches = [regex]::Matches($batchRppText, '(?m)^\s*LENGTH\s+([-+0-9.eE]+)')
$rppPass = $positionMatches.Count -eq 2 -and $lengthMatches.Count -eq 2 -and
    (Number $positionMatches[0].Groups[1].Value) -ge 30.0 -and
    (Number $positionMatches[1].Groups[1].Value) -ge
        ((Number $positionMatches[0].Groups[1].Value) + (Number $lengthMatches[0].Groups[1].Value))

$report = [ordered]@{
    schema = 1
    completed_utc = [DateTime]::UtcNow.ToString('o')
    reaper_exe = $reaperExe
    isolated_host = [bool]$IsolatedHost
    alternate_resource_config = $alternateConfig
    pipe_name = $pipeName
    candidate_dll = $candidate
    candidate_sha256 = $candidateHash
    installed_sha256 = $installedHash
    installed_file_version = (Get-Item -LiteralPath $installedDll).VersionInfo.FileVersion
    single = [ordered]@{ pass = $singlePass; client = $singleOutput; row = $singleRow }
    batch = [ordered]@{ pass = $batchPass; client = $batchOutput; row = $batchRow }
    failure_rollback = [ordered]@{
        pass = $failurePass
        client_exit_code = $failureExitCode
        client = $failureOutput
        row = $failureRow
    }
    parsed_rpp = [ordered]@{ pass = $rppPass; positions = @($positionMatches.Value); lengths = @($lengthMatches.Value) }
    graceful_exit = $process.HasExited
    run_root = $runRoot
}
$reportPath = Join-Path $runRoot 'report.json'
[IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 8), $utf8)
if (-not ($singlePass -and $batchPass -and $failurePass -and $rppPass)) {
    throw "真实 REAPER 验收断言失败，详见：$reportPath"
}
Write-Output "HARNESS-REAPER-REAL PASS：$reportPath"
