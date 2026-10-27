param(
    [Parameter(Mandatory=$true)][string]$CandidateDirectory,
    [Parameter(Mandatory=$true)][string]$ExpectedSha256,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateRange(10,120)][int]$CaptureSeconds=60
)
# 仅用于一次性 Windows Sandbox：正常启动，不使用会跳过欢迎页的预览环境。
# 输出连续截图和原生窗口身份；截图中的页面内容仍须人工核对。
$ErrorActionPreference='Stop'
if($env:USERNAME -ne 'WDAGUtilityAccount'){throw 'Run only inside a disposable Windows Sandbox.'}
if(Get-Process HarkenSearch -ErrorAction SilentlyContinue){throw 'Harken Search must fully exit before cold-start capture.'}
$exe=Join-Path (Resolve-Path -LiteralPath $CandidateDirectory).Path 'HarkenSearch.exe'
if((Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant() -ne $ExpectedSha256.ToLowerInvariant()){throw 'Candidate SHA256 does not match.'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Use a new evidence directory; previous evidence is immutable.'}
$profile=Join-Path $env:LOCALAPPDATA 'HarkenSearch'
if(Test-Path -LiteralPath $profile){throw 'Use a fresh sandbox; do not overwrite an existing profile.'}
New-Item -ItemType Directory -Path $OutputDirectory,$profile | Out-Null
# 合成旧布局：故意保留导致主界面脱离宿主窗口的 ViewportId。
$fixture=@('[Window][MainWindow]','ViewportPos=0,23','ViewportId=0x72FC8CA3','Size=2560,1369','Collapsed=0','') -join [Environment]::NewLine
[IO.File]::WriteAllText((Join-Path $profile 'imgui.ini'),$fixture,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllLines((Join-Path $profile 'settings.ini'),@('language=0','closeButtonAction=2'),[Text.UTF8Encoding]::new($false))
$fixture | Set-Content -LiteralPath (Join-Path $OutputDirectory 'synthetic-imgui.ini') -Encoding ASCII
foreach($name in @('AURA_RUNTIME_ROOT','AURA_DISABLE_SINGLE_INSTANCE','AURA_HARNESS_MULTI_INSTANCE','AURO_OPEN_UI_GALLERY','ARIS_PREVIEW_ISOLATION','WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS')){
    [Environment]::SetEnvironmentVariable($name,$null,'Process')
}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
public sealed class SingleHostWindow {
    public long hwnd; public uint pid; public string title; public string windowClass;
    public long owner; public long exStyle; public int left; public int top; public int width; public int height;
    public bool taskbarCandidate;
}
public static class SingleHostProbe {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc,IntPtr data);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int size);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd,StringBuilder text,int size);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr hwnd,uint cmd);
    [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] static extern IntPtr GetWindowLongPtr(IntPtr hwnd,int index);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    public static SingleHostWindow[] Windows(uint expectedPid) {
        var result=new List<SingleHostWindow>();
        EnumWindows(delegate(IntPtr hwnd,IntPtr data) {
            uint pid; GetWindowThreadProcessId(hwnd,out pid);
            if(pid!=expectedPid || !IsWindowVisible(hwnd))return true;
            Rect rect; GetWindowRect(hwnd,out rect);
            if(rect.right<=rect.left || rect.bottom<=rect.top)return true;
            var title=new StringBuilder(512); GetWindowText(hwnd,title,title.Capacity);
            var cls=new StringBuilder(256); GetClassName(hwnd,cls,cls.Capacity);
            long owner=GetWindow(hwnd,4).ToInt64(); long style=GetWindowLongPtr(hwnd,-20).ToInt64();
            result.Add(new SingleHostWindow { hwnd=hwnd.ToInt64(),pid=pid,title=title.ToString(),windowClass=cls.ToString(),owner=owner,exStyle=style,left=rect.left,top=rect.top,width=rect.right-rect.left,height=rect.bottom-rect.top,taskbarCandidate=(style&0x80)==0 && (owner==0 || (style&0x40000)!=0) });
            return true;
        },IntPtr.Zero);
        return result.ToArray();
    }
    public static void Capture(SingleHostWindow window,string path) {
        using(var bitmap=new Bitmap(window.width,window.height)) {
            using(var graphics=Graphics.FromImage(bitmap)) {
                graphics.CopyFromScreen(window.left,window.top,0,0,new Size(window.width,window.height));
            }
            bitmap.Save(path,ImageFormat.Png);
        }
    }
}
'@
[void][SingleHostProbe]::SetProcessDPIAware()
$record=[ordered]@{startedUtc=[DateTime]::UtcNow.ToString('o');exe=$exe;exeSha256=$ExpectedSha256;arguments=@();syntheticProfile=$profile;startupBypass=$false;pid=0;frames=@();windowIdentityPassed=$false;visualReview='pending'}
$frames=[Collections.Generic.List[object]]::new()
$app=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Normal -PassThru
$record.pid=$app.Id
$timer=[Diagnostics.Stopwatch]::StartNew()
$sequence=0
while($timer.Elapsed.TotalSeconds -lt $CaptureSeconds){
    $app.Refresh()
    if($app.HasExited){$record.exitCode=$app.ExitCode;break}
    $windows=@([SingleHostProbe]::Windows([uint32]$app.Id))
    $time=[Math]::Round($timer.Elapsed.TotalSeconds,3)
    $entry=[ordered]@{t=$time;windows=$windows;screenshots=@()}
    foreach($window in $windows){
        if($window.width -ge 400 -and $window.height -ge 200 -and $window.width -le 4096 -and $window.height -le 2160){
            $name=('frame-{0:D4}-{1}.png' -f $sequence,$window.hwnd)
            [SingleHostProbe]::Capture($window,(Join-Path $OutputDirectory $name))
            $entry.screenshots+= $name
        }
    }
    $frames.Add([pscustomobject]$entry)
    ++$sequence
    Start-Sleep -Milliseconds 120
}
$record.frames=@($frames.ToArray())
$substantial=@($record.frames | ForEach-Object {$_.windows} | Where-Object {$_.width -ge 400 -and $_.height -ge 200})
$record.hostHandles=@($substantial | Select-Object -ExpandProperty hwnd -Unique)
$record.splitMainWindows=@($substantial | Where-Object {$_.title -eq 'MainWindow'}).Count
$record.maxTaskbarWindows=(@($record.frames | ForEach-Object {@($_.windows | Where-Object {$_.taskbarCandidate}).Count}) | Measure-Object -Maximum).Maximum
$record.finalProcessPath=$app.Path
$record.finalExeSha256=(Get-FileHash -LiteralPath $app.Path).Hash.ToLowerInvariant()
$record.windowIdentityPassed=($record.hostHandles.Count -eq 1 -and $record.splitMainWindows -eq 0 -and $record.maxTaskbarWindows -eq 1 -and $record.finalExeSha256 -eq $ExpectedSha256.ToLowerInvariant() -and $substantial.Count -gt 10 -and -not $app.HasExited)
$record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'capture.json') -Encoding UTF8
if(-not $record.windowIdentityPassed){exit 1}
