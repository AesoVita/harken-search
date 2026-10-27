"""Read-only native corner check for an explicitly identified Windows QA window.

Example: python tools/qa_floating_shell_corners.py --pid 123 --hwnd 456
         --exe D:/Aris-QA/card/package/HarkenSearch.exe --output evidence.json
No mouse input, window changes, or global process discovery.
"""
import argparse
import ctypes
import json
from ctypes import wintypes
from pathlib import Path


def inspect_corners(pid, hwnd, exe):
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.GetWindowRgn.argtypes = [wintypes.HWND, wintypes.HRGN]
    gdi.CreateRectRgn.argtypes = [ctypes.c_int] * 4
    gdi.CreateRectRgn.restype = wintypes.HRGN
    gdi.PtInRegion.argtypes = [wintypes.HRGN, ctypes.c_int, ctypes.c_int]
    gdi.DeleteObject.argtypes = [wintypes.HGDIOBJ]
    process = kernel.OpenProcess(0x1000, False, pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        path = ctypes.create_unicode_buffer(32768)
        length = wintypes.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(process, 0, path, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        assert Path(path.value).resolve() == Path(exe).resolve(), 'EXE identity mismatch'
    finally:
        kernel.CloseHandle(process)
    owner = wintypes.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
    assert owner.value == pid, 'HWND belongs to another process'
    rect = wintypes.RECT()
    assert user.GetWindowRect(hwnd, ctypes.byref(rect)), 'Window rectangle unavailable'
    width, height = rect.right - rect.left, rect.bottom - rect.top
    assert width > 32 and height > 32
    region = gdi.CreateRectRgn(0, 0, 0, 0)
    if not region:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        kind = user.GetWindowRgn(hwnd, region)
        # GetWindowRgn returns ERROR when no custom region is set: the HWND
        # then occupies its full rectangle. Native DWM effects are not inferred.
        points = {'bottomLeft':(0,height-1),'bottomRight':(width-1,height-1),
                  'topLeft':(0,0),'topRight':(width-1,0),
                  'center':(width//2,height//2),'bottomCenter':(width//2,height-2),
                  'leftCenter':(1,height//2),'rightCenter':(width-2,height//2)}
        contained = {name:bool(gdi.PtInRegion(region,*point)) if kind else True
                     for name,point in points.items()}
    finally:
        gdi.DeleteObject(region)
    passed = (kind == 3 and not any(contained[k] for k in ('bottomLeft','bottomRight','topLeft','topRight'))
              and all(contained[k] for k in ('center','bottomCenter','leftCenter','rightCenter')))
    return {'pid':pid,'hwnd':hwnd,'exe':str(Path(exe).resolve()),'size':[width,height],
            'regionType':kind,'contained':contained,'passed':passed}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--hwnd',type=int,required=True)
    parser.add_argument('--exe',required=True)
    parser.add_argument('--output',type=Path)
    args = parser.parse_args()
    result = inspect_corners(args.pid,args.hwnd,args.exe)
    report = json.dumps(result,ensure_ascii=False,indent=2)
    if args.output:
        args.output.write_text(report,encoding='utf-8')
    print(report)
    raise SystemExit(0 if result['passed'] else 1)
