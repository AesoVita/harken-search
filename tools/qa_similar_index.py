"""RQ-20261022-01: CLI and native screenshots for the isolated real QA EXE.

Requires the QA build pinned to ROOT/data. Never uses the installed app/profile.
Examples: --fixtures; --action open; --command-file request.json;
          --capture empty --pid <verified QA process ID>
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import math
from pathlib import Path
import sqlite3
import struct
import time
import uuid
import wave

ROOT = Path("D:/Aris-QA/RQ-20261022-01-similar-panel")
DATA = ROOT / "data"
RUNTIME = DATA / "profile/runtime"
EVIDENCE = ROOT / "evidence"


def command(payload: dict, timeout: float = 20) -> dict:
    request_id = uuid.uuid4().hex
    payload = dict(payload, request_id=request_id)
    cmd = RUNTIME / "qa_cmd.json"
    if cmd.exists():
        raise RuntimeError("Previous QA command has not been consumed")
    tmp = RUNTIME / f"qa_cmd.{request_id}.tmp"
    tmp.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
    tmp.replace(cmd)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            result = json.loads((RUNTIME / "qa_result.json").read_text(encoding="utf-8"))
            if result.get("request_id") == request_id:
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                with (EVIDENCE / "commands.jsonl").open("a", encoding="utf-8") as out:
                    out.write(json.dumps({"request": payload, "result": result}, ensure_ascii=False) + "\n")
                if not result.get("ok"):
                    raise RuntimeError(result)
                return result
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        time.sleep(0.05)
    raise TimeoutError(payload)


def fixtures() -> None:
    RUNTIME.mkdir(parents=True, exist_ok=True)
    audio = DATA / "synthetic-audio"
    audio.mkdir(exist_ok=True)
    for name in ("fixture-library", "empty-library"):
        db = DATA / name / "main.db"
        if db.exists():
            continue
        db.parent.mkdir(exist_ok=True)
        with sqlite3.connect(db) as connection:
            connection.execute("PRAGMA user_version=1")
    sample_rate = 22050
    # All files are generated tones: no user media or library is read.
    for index in range(160):
        path = audio / f"qa-tone-{index:03}.wav"
        if path.exists():
            continue
        frequency = 90 + index * 11
        second = b"".join(struct.pack("<h", int(9000 * math.sin(2 * math.pi * frequency * n / sample_rate)))
                          for n in range(sample_rate))
        with wave.open(str(path), "wb") as out:
            out.setparams((1, 2, sample_rate, 0, "NONE", "not compressed"))
            out.writeframes(second * 20)
    settings = RUNTIME / "settings_dev.ini"
    if not settings.exists():
        settings.write_text("currentDbPath=" + (DATA / "fixture-library/main.db").as_posix() +
                            "\nlanguage=0\ndiagnosticMode=1\n", encoding="utf-8")
    # Synthetic test persistence, not a user or installed-product acceptance.
    consent_fixture = settings.with_name(settings.name + ".legal-consent")
    if not consent_fixture.exists():
        consent_fixture.write_text("eulaVersion=1\nprivacyVersion=1\n"
                                   "acceptedAtUtc=QA-FIXTURE-NOT-USER-CONSENT\n"
                                   "appVersion=RQ-20261022-01-test-fixture\n", encoding="utf-8")
    (ROOT / "qa/dev_mode.txt").write_text("RQ-20261022-01 isolated QA\n", encoding="utf-8")


def capture(name: str, pid: int) -> list[str]:
    from PIL import ImageGrab
    if not name.replace("-", "").replace("_", "").isalnum():
        raise ValueError("Capture name must be a plain file stem")
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel32.OpenProcess(0x1000, False, pid)
    path = ctypes.create_unicode_buffer(32768)
    length = wintypes.DWORD(len(path))
    try:
        if not handle or not kernel32.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(length)):
            raise OSError("Cannot verify QA process path")
        if Path(path.value).resolve() != (ROOT / "qa/HarkenSearch.exe").resolve():
            raise RuntimeError("Process is not this card's QA executable")
    finally:
        if handle:
            kernel32.CloseHandle(handle)
    windows = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def visit(hwnd, _):
        window_pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(window_pid))
        if window_pid.value == pid and user32.IsWindowVisible(hwnd):
            title = ctypes.create_unicode_buffer(1024)
            user32.GetWindowTextW(hwnd, title, len(title))
            if title.value and title.value != "Default IME":
                windows.append((hwnd, title.value))
        return True

    user32.EnumWindows(visit, 0)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    paths = []
    for index, (hwnd, title) in enumerate(windows):
        output = EVIDENCE / f"{name}-{index}.png"
        ImageGrab.grab(window=hwnd).save(output)
        paths.append(str(output))
    (EVIDENCE / f"{name}-windows.json").write_text(json.dumps(windows, ensure_ascii=False), encoding="utf-8")
    return paths


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", action="store_true")
    parser.add_argument("--command")
    parser.add_argument("--command-file", type=Path)
    parser.add_argument("--action", choices=["open", "close", "start", "cancel", "find", "state"])
    parser.add_argument("--capture")
    parser.add_argument("--pid", type=int)
    args = parser.parse_args()
    if args.fixtures:
        fixtures()
        print("Synthetic fixtures ready")
    if args.command:
        print(json.dumps(command(json.loads(args.command)), ensure_ascii=False))
    if args.command_file:
        print(json.dumps(command(json.loads(args.command_file.read_text(encoding="utf-8"))), ensure_ascii=False))
    if args.action:
        print(json.dumps(command({"cmd": "similar_index", "action": args.action}), ensure_ascii=False))
    if args.capture:
        print(json.dumps(capture(args.capture, args.pid), ensure_ascii=False))
