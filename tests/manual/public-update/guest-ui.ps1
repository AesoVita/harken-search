$ErrorActionPreference = 'Stop'
if ($env:USERNAME -ne 'WDAGUtilityAccount') { throw 'This helper only runs in the disposable Windows Sandbox.' }
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SandboxInput {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
}
'@
$lastId = ''
while (-not (Test-Path -LiteralPath 'C:/qa-input/stop-helper')) {
  Start-Sleep -Milliseconds 400
  if (-not (Test-Path -LiteralPath 'C:/qa-input/ui-command.json')) { continue }
  try { $command = [System.IO.File]::ReadAllText('C:/qa-input/ui-command.json') | ConvertFrom-Json } catch { continue }
  if ($command.id -eq $lastId) { continue }
  $lastId = $command.id
  try {
    if ($command.kind -eq 'click') {
      $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
      if ($command.x -lt 0 -or $command.y -lt 0 -or $command.x -ge $bounds.Width -or $command.y -ge $bounds.Height) { throw 'Click is outside sandbox screen' }
      [void][SandboxInput]::SetCursorPos([int]$command.x,[int]$command.y)
      [SandboxInput]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
      [SandboxInput]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
    } elseif ($command.kind -eq 'close-app') {
      Get-Process HarkenSearch -ErrorAction SilentlyContinue | ForEach-Object { [void]$_.CloseMainWindow() }
    } elseif ($command.kind -ne 'screenshot') { throw 'Unknown sandbox UI operation' }
    Start-Sleep -Milliseconds 700
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object Drawing.Bitmap($bounds.Width,$bounds.Height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location,[Drawing.Point]::Empty,$bounds.Size)
    $bitmap.Save('C:/qa-output/guest-latest.png',[Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose(); $bitmap.Dispose()
    @{id=$lastId;kind=$command.kind;status='done';observedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding UTF8 'C:/qa-output/ui-receipt.json'
  } catch {
    @{id=$lastId;status='failed';error=$_.Exception.Message} | ConvertTo-Json | Set-Content -Encoding UTF8 'C:/qa-output/ui-receipt.json'
  }
}
