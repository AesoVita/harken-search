"""真实安装包界面复验，在实际安装前取消。依赖 Windows、pywin32、Pillow。

仅操作本次启动的进程及子进程，不点击安装，不更改系统语言。
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import re
import subprocess
import time

import win32con
import win32gui
import win32process
from PIL import ImageGrab

ROOT = Path(__file__).resolve().parents[1]
USER32 = ctypes.WinDLL('user32', use_last_error=True)
USER32.SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_void_p]
USER32.SendMessageW.restype = ctypes.c_ssize_t


def text_of(hwnd):
    n = USER32.SendMessageW(hwnd, win32con.WM_GETTEXTLENGTH, 0, None)
    buffer = ctypes.create_unicode_buffer(n + 1)
    USER32.SendMessageW(hwnd, win32con.WM_GETTEXT, n + 1, ctypes.cast(buffer, ctypes.c_void_p))
    return buffer.value


def clean(text):
    return text.replace('&', '').replace('<', '').replace('>', '').strip()


def paragraphs(text):
    return [line.strip() for line in text.splitlines() if line.strip()]


class Preview:
    def __init__(self, args):
        self.exe = args.installer.resolve(strict=True)
        assert hashlib.sha256(self.exe.read_bytes()).hexdigest() == args.sha256.lower(), '安装包摘要不匹配'
        self.out = args.evidence_dir.resolve()
        self.out.mkdir(parents=True, exist_ok=True)
        self.ui_language_id = ctypes.windll.kernel32.GetUserDefaultUILanguage()
        self.language = args.language
        if self.language == 'auto':
            self.language = 'chinesesimp' if self.ui_language_id in (0x0804, 0x1004) else 'english'
        self.zh = self.language == 'chinesesimp'
        self.next_text = '下一步(N)' if self.zh else 'Next'
        self.cancel_text = '取消' if self.zh else 'Cancel'
        command = [str(self.exe), '/SP-', '/CURRENTUSER', '/NOICONS',
                   '/DIR=' + str(self.out / 'not-installed'), '/LOG=' + str(self.out / 'installer.log')]
        if args.language != 'auto':
            command.append('/LANG=' + args.language)
        self.process = subprocess.Popen(command, cwd=self.exe.parent)
        self.pids = {self.process.pid}
        self.receipt = {'package': str(self.exe), 'sha256': args.sha256, 'command': command,
                        'windowsUiLanguageId': hex(self.ui_language_id), 'expectedLanguage': self.language,
                        'launcherPid': self.process.pid, 'installed': False, 'checks': []}
        self.save()
        time.sleep(2)

    def save(self):
        (self.out / 'result.json').write_text(json.dumps(self.receipt, ensure_ascii=False, indent=2), encoding='utf-8')

    def windows(self):
        command = '(Get-CimInstance Win32_Process | Select-Object ProcessId,ParentProcessId,ExecutablePath) | ConvertTo-Json -Compress'
        result = subprocess.run(['powershell', '-NoProfile', '-Command', command], capture_output=True, text=True, check=True)
        procs = json.loads(result.stdout)
        launcher = next((p for p in procs if p['ProcessId'] == self.process.pid), None)
        if launcher:
            assert launcher['ExecutablePath'] and Path(launcher['ExecutablePath']).resolve() == self.exe
        for _ in range(5):
            self.pids.update(p['ProcessId'] for p in procs if p['ParentProcessId'] in self.pids)
        found = []
        def visit(hwnd, _):
            pid = win32process.GetWindowThreadProcessId(hwnd)[1]
            if pid not in self.pids or not win32gui.IsWindowVisible(hwnd):
                return
            kind = win32gui.GetClassName(hwnd)
            if kind not in ('TWizardForm', 'TNewMessageForm', '#32770'):
                return
            children = []
            def child(ch, _):
                if win32gui.IsWindowVisible(ch):
                    children.append({'hwnd': ch, 'class': win32gui.GetClassName(ch), 'text': text_of(ch),
                                     'enabled': bool(win32gui.IsWindowEnabled(ch))})
            win32gui.EnumChildWindows(hwnd, child, None)
            found.append({'hwnd': hwnd, 'pid': pid, 'class': kind, 'title': text_of(hwnd), 'children': children})
        win32gui.EnumWindows(visit, None)
        return found

    def wizard(self):
        return next(w for w in self.windows() if w['class'] == 'TWizardForm')

    def control(self, kind):
        return next(c for c in self.wizard()['children'] if c['class'] == kind)

    def click(self, control):
        assert control['enabled']
        current = {c['hwnd']: c for w in self.windows() for c in w['children']}
        assert control == current[control['hwnd']], '操作前窗口已改变'
        if control['class'] != 'TNewCheckBox':
            label = clean(control['text'])
            assert label in ('Next', 'Back', 'Cancel', 'OK', 'Yes', '下一步(N)', '上一步(B)',
                             '取消', '确定', '是(Y)', '是'), '拒绝操作非预览按钮：' + label
        win32gui.PostMessage(control['hwnd'], win32con.WM_LBUTTONDOWN, win32con.MK_LBUTTON, 655370)
        win32gui.PostMessage(control['hwnd'], win32con.WM_LBUTTONUP, 0, 655370)
        time.sleep(0.25)

    def button(self, label, window=None):
        window = window or self.wizard()
        return next(c for c in window['children'] if clean(c['text']) == label and c['class'] in ('TNewButton', 'Button'))

    def capture(self, name, window=None):
        window = window or self.wizard()
        hwnd = window['hwnd']
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        win32gui.SetWindowPos(hwnd, win32con.HWND_TOP, 100, 80, 0, 0, win32con.SWP_NOSIZE)
        win32gui.RedrawWindow(hwnd, None, None, win32con.RDW_INVALIDATE | win32con.RDW_UPDATENOW | win32con.RDW_ALLCHILDREN)
        time.sleep(0.2)
        ImageGrab.grab(window=hwnd).save(self.out / (name + '.png'))
        (self.out / (name + '-controls.json')).write_text(json.dumps(window, ensure_ascii=False, indent=2), encoding='utf-8')

    def dismiss_error(self, expected, name):
        dialog = next(w for w in self.windows() if w['class'] != 'TWizardForm')
        content = chr(10).join(c['text'] for c in dialog['children'])
        assert expected in content, content
        self.capture(name, dialog)
        # Windows 原生消息框的按钮跟随系统语言，强制 /LANG 不改系统语言。
        ok = next(c for c in dialog['children'] if clean(c['text']) in ('确定', 'OK'))
        self.click(ok)

    def legal_page(self, base):
        memo = self.control('TNewMemo')
        expected = (ROOT / 'docs/legal' / (base + ('.md' if self.zh else '.en.md'))).read_text(encoding='utf-8')
        assert paragraphs(memo['text']) == paragraphs(expected), base + ' 正文与仓库原稿不一致'
        assert 'PLACEHOLDER' not in memo['text']
        window = self.wizard()
        assert ('安装 -' if self.zh else 'Setup -') in window['title'], window['title']
        self.button(self.next_text, window)
        self.button(self.cancel_text, window)
        if not self.zh:
            assert not re.search('[一-鿿]', chr(10).join(c['text'] for c in window['children']))
        self.capture(base.lower() + '-top', window)
        read_error = '滚动到底部' if self.zh else 'scroll to the bottom'
        consent_error = '必须同意' if self.zh else 'You must agree'
        self.click(self.button(self.next_text))
        self.dismiss_error(read_error, base.lower() + '-read-required')
        self.click(self.control('TNewCheckBox'))
        self.dismiss_error(read_error, base.lower() + '-early-check-rejected')
        win32gui.SendMessage(memo['hwnd'], win32con.WM_VSCROLL, win32con.SB_BOTTOM, 0)
        time.sleep(0.2)
        self.click(self.button(self.next_text))
        self.dismiss_error(consent_error, base.lower() + '-consent-required')
        self.click(self.control('TNewCheckBox'))
        self.capture(base.lower() + '-accepted')
        self.click(self.button(self.next_text))
        self.receipt['checks'].append({'document': base, 'paragraphsMatch': True, 'readingRequired': True,
                                       'earlyCheckRejected': True, 'agreementRequired': True, 'acceptedCanContinue': True})
        self.save()

    def run(self):
        if not any(c['class'] == 'TNewMemo' for c in self.wizard()['children']):
            self.click(self.button(self.next_text))
        self.legal_page('EULA')
        self.legal_page('PRIVACY_POLICY')
        self.capture('destination')
        # 实际安装按钮不在 click 的许可集合中。
        for i in range(5):
            w = self.wizard()
            if any(clean(c['text']) in ('Install', '安装(I)') for c in w['children']):
                self.capture('ready', w)
                self.receipt['readyReached'] = True
                break
            self.click(self.button(self.next_text, w))
            self.capture('options-' + str(i))
        assert self.receipt.get('readyReached'), '未到达准备安装页'
        self.click(self.button(self.cancel_text))
        dialog = next((w for w in self.windows() if w['class'] != 'TWizardForm'), None)
        if dialog:
            self.capture('cancel-confirm', dialog)
            yes = next(c for c in dialog['children'] if clean(c['text']) in ('Yes', '是(Y)', '是'))
            self.click(yes)
        self.process.wait(timeout=15)
        assert not self.windows(), '预览窗口未关闭'
        self.receipt['status'] = 'passed'
        self.receipt['closed'] = True
        self.save()
        print(json.dumps(self.receipt, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', type=Path, required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    parser.add_argument('--language', choices=('auto', 'chinesesimp', 'english'), default='auto')
    preview = Preview(parser.parse_args())
    try:
        preview.run()
    except Exception as error:
        preview.receipt['status'] = 'failed'
        preview.receipt['error'] = str(error)
        preview.save()
        raise


if __name__ == '__main__':
    main()
