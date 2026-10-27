param(
    [string]$InstallRoot = "C:\Program Files\Avid\Pro Tools"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AuroWindowProbe {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
}
"@

$proToolsExe = Join-Path $InstallRoot "ProTools.exe"
$ptsl = Join-Path $InstallRoot "PTSL.dll"
$ptAaf = Join-Path $InstallRoot "PtAAF.dll"
$aaf = Join-Path $InstallRoot "AAFCOAPI.dll"

$liveClasses = @("DigiAppWndClass", "DigiMDIWndClass", "AvidEditorMainWndClass")
$activationClasses = @("PACEEdenExperienceProxy")
$seenLive = New-Object System.Collections.Generic.List[string]
$seenActivation = New-Object System.Collections.Generic.List[string]

$callback = [AuroWindowProbe+EnumWindowsProc]{
    param([IntPtr]$hwnd, [IntPtr]$lparam)
    if (-not [AuroWindowProbe]::IsWindowVisible($hwnd)) { return $true }
    $sb = New-Object System.Text.StringBuilder 256
    [void][AuroWindowProbe]::GetClassName($hwnd, $sb, $sb.Capacity)
    $className = $sb.ToString()
    if ($liveClasses -contains $className) { $seenLive.Add($className) }
    if ($activationClasses -contains $className) { $seenActivation.Add($className) }
    return $true
}
[void][AuroWindowProbe]::EnumWindows($callback, [IntPtr]::Zero)

$process = Get-Process -Name "ProTools" -ErrorAction SilentlyContinue
$exePresent = Test-Path -LiteralPath $proToolsExe
$ptslPresent = Test-Path -LiteralPath $ptsl
$ptAafPresent = Test-Path -LiteralPath $ptAaf
$aafPresent = Test-Path -LiteralPath $aaf

$readiness = "notInstalled"
$failure = "proToolsNotInstalled"
if ($exePresent) {
    if ($seenActivation.Count -gt 0 -and $seenLive.Count -eq 0) {
        $readiness = "installedButHostNotReady"
        $failure = "activationGate"
    } elseif ($ptslPresent -and $seenLive.Count -gt 0) {
        $readiness = "livePtslPossible"
        $failure = "none"
    } elseif (-not $ptslPresent) {
        $readiness = "installedButHostNotReady"
        $failure = "ptslMissing"
    } elseif ($ptAafPresent -and $aafPresent) {
        $readiness = "aafFallbackPossible"
        $failure = "hostWindowUnavailable"
    } else {
        $readiness = "installedButHostNotReady"
        $failure = "aafMissing"
    }
}

$result = [ordered]@{
    protocol = "auro-protools-host-send-v1"
    installRoot = $InstallRoot
    proToolsExePresent = $exePresent
    ptslPresent = $ptslPresent
    ptAafPresent = $ptAafPresent
    aafCoApiPresent = $aafPresent
    proToolsProcessRunning = [bool]$process
    liveHostWindowClasses = @($seenLive | Select-Object -Unique)
    activationWindowClasses = @($seenActivation | Select-Object -Unique)
    readiness = $readiness
    failure = $failure
    note = "livePtslPossible is only a preflight condition; it is not proof of timeline insertion. AAF fallback must not be reported as live spot."
}

$result | ConvertTo-Json -Depth 5
if ($readiness -eq "livePtslPossible") {
    exit 0
}
exit 2
