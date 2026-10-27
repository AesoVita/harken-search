from pathlib import Path
import ctypes
import json
import os
import re
import shutil
import subprocess
import time
import unittest

SOURCE_ROOT = Path(__file__).resolve().parents[1]
PREVIEW_ROOT = SOURCE_ROOT.parent
APPROVED_ROOT = Path(r'D:/Aris-QA/test-databases/RQ-20261010-NONDOCK-PREVIEW-installer-identity')
LOGS_DIR = PREVIEW_ROOT / 'logs' / 'batch11-installer-identity'
FILE_ATTRIBUTE_REPARSE_POINT = 0x400
INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF
FORBIDDEN_BASENAMES = {
    'HarkenSearch.exe',
    'HarkenSearchBootstrap.exe',
    'signtool.exe',
    'unins000.exe',
    'HarkenSearch.exe.sig',
    'HarkenSearchBootstrap.exe.sig',
    'HarkenSearchProToolsBridge.exe',
}
ISCC_CANDIDATES = (
    Path(r'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'),
    Path(r'C:/Program Files/Inno Setup 6/ISCC.exe'),
)


def _file_attributes(path: Path) -> int:
    return ctypes.windll.kernel32.GetFileAttributesW(str(path))


def _is_reparse(path: Path) -> bool:
    attributes = _file_attributes(path)
    if attributes == INVALID_FILE_ATTRIBUTES:
        return False
    return bool(attributes & FILE_ATTRIBUTE_REPARSE_POINT)


def _absolute(path: Path) -> Path:
    return Path(os.path.abspath(str(path)))


def _inside_approved(path: Path) -> bool:
    approved = str(_absolute(APPROVED_ROOT))
    text = str(_absolute(path))
    return text == approved or text.startswith(approved + os.sep)


def _reparse_in_existing_chain(path: Path):
    cursor = _absolute(path)
    while True:
        if cursor.exists() and _is_reparse(cursor):
            return cursor
        if cursor.parent == cursor:
            return None
        cursor = cursor.parent


def _inspect_path(path: Path, must_exist: bool):
    if not _inside_approved(path):
        return False, _absolute(path), 'outside-approved-root'
    marked = _reparse_in_existing_chain(path)
    if marked is not None:
        return False, _absolute(path), 'reparse:%s' % marked
    if must_exist and not path.exists():
        return False, _absolute(path), 'missing'
    return True, _absolute(path), 'ok'


def _find_iscc():
    for candidate in ISCC_CANDIDATES:
        if candidate.exists() and candidate.is_file():
            return candidate
    return None


def _write_bytes(path: Path, payload: bytes) -> None:
    ok, resolved, reason = _inspect_path(path, must_exist=False)
    if not ok:
        raise RuntimeError('write-rejected:%s:%s' % (path, reason))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    ok, resolved, reason = _inspect_path(path, must_exist=True)
    if not ok:
        raise RuntimeError('write-postcheck:%s:%s' % (path, reason))
    if path.name in FORBIDDEN_BASENAMES:
        raise RuntimeError('forbidden-basename:%s' % path.name)


def _copy_static(src: Path, dest: Path) -> None:
    ok, resolved, reason = _inspect_path(dest, must_exist=False)
    if not ok:
        raise RuntimeError('copy-rejected:%s:%s' % (dest, reason))
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dest)
    ok, resolved, reason = _inspect_path(dest, must_exist=True)
    if not ok:
        raise RuntimeError('copy-postcheck:%s:%s' % (dest, reason))
    if dest.name in FORBIDDEN_BASENAMES:
        raise RuntimeError('forbidden-basename:%s' % dest.name)


class InstallerScriptCompileTest(unittest.TestCase):
    def setUp(self) -> None:
        self.iscc = _find_iscc()
        stamp = time.strftime('%Y%m%d-%H%M%S')
        self.run_dir = APPROVED_ROOT / ('probe-%s-%s' % (stamp, os.getpid()))
        self.created_names = []

    def _ensure_dirs(self):
        ok, resolved, reason = _inspect_path(APPROVED_ROOT, must_exist=False)
        self.assertTrue(ok, reason)
        APPROVED_ROOT.mkdir(parents=True, exist_ok=True)
        ok, resolved, reason = _inspect_path(APPROVED_ROOT, must_exist=True)
        self.assertTrue(ok, reason)
        ok, resolved, reason = _inspect_path(self.run_dir, must_exist=False)
        self.assertTrue(ok, reason)
        names = ('payload', 'legal', 'icon', 'tool', 'compiler-temp', 'output-plain', 'output-conditional')
        dirs = {'run': self.run_dir}
        for name in names:
            path = self.run_dir / name
            ok, resolved, reason = _inspect_path(path, must_exist=False)
            self.assertTrue(ok, reason)
            path.mkdir(parents=True, exist_ok=True)
            ok, resolved, reason = _inspect_path(path, must_exist=True)
            self.assertTrue(ok, reason)
            dirs[name] = path
        return dirs

    def _rewrite_script(self, dirs, output_key, output_basename):
        product = (SOURCE_ROOT / 'installer' / 'HarkenSearch_Setup.iss').read_text(encoding='utf-8')
        self.assertIn('function CreateFileW', product)
        self.assertIn('IsTrustedLegacyUninstaller', product)
        self.assertIn('procedure CurStepChanged', product)
        payload_dir = dirs['payload']
        mapping = {}
        payload_index = 0
        rewritten = []
        source_re = re.compile(r'Source:\s*"([^"]+)"')
        for line in product.splitlines():
            match = source_re.search(line)
            if match is None:
                rewritten.append(line)
                continue
            original = match.group(1)
            if original.startswith('{#SignToolPath}'):
                rewritten.append(line)
                continue
            if original.startswith('{#SourceDir}\\') or original.startswith('{#SourceDir}/'):
                rest = original[len('{#SourceDir}\\'):] if original.startswith('{#SourceDir}\\') else original.split('/', 1)[1]
                if rest.replace('\\', '/') == 'docs/HarkenSearch-Manual-zh-CN.pdf':
                    manual = payload_dir / 'docs/HarkenSearch-Manual-zh-CN.pdf'
                    _copy_static(SOURCE_ROOT / 'docs/manual/HarkenSearch-Manual-zh-CN.pdf', manual)
                    self.assertNotIn('skipifsourcedoesntexist', line)
                    new_source = original
                elif '*' in rest:
                    folder = 'fixture_assets' if 'assets' in rest.lower() else 'fixture_licenses'
                    target_dir = payload_dir / folder
                    _write_bytes(target_dir / 'fixture_placeholder.bin', b'fixture-dir')
                    new_source = '{#SourceDir}\\' + folder + '\\*'
                else:
                    payload_index += 1
                    new_name = 'fixture_payload_%02d.bin' % payload_index
                    _write_bytes(payload_dir / new_name, b'fixture-payload')
                    new_source = '{#SourceDir}\\' + new_name
                mapping[original] = new_source
                rewritten.append(line.replace(original, new_source, 1))
                continue
            if 'docs\\legal\\' in original or 'docs/legal/' in original:
                name = original.replace('\\', '/').rsplit('/', 1)[1]
                dest = dirs['legal'] / name
                _copy_static(SOURCE_ROOT / 'docs' / 'legal' / name, dest)
                new_source = str(dest)
                mapping[original] = new_source
                rewritten.append(line.replace(original, new_source, 1))
                continue
            rewritten.append(line)
        script = '\n'.join(rewritten) + '\n'
        icon_dest = dirs['icon'] / 'fixture_setup_icon.ico'
        _copy_static(SOURCE_ROOT / 'HarkenSearch.ico', icon_dest)
        language_dest = dirs['legal'] / 'ChineseSimplified.isl'
        _copy_static(SOURCE_ROOT / 'installer/Languages/ChineseSimplified.isl', language_dest)
        script = script.replace('MessagesFile: "Languages\\ChineseSimplified.isl"',
                                'MessagesFile: "%s"' % language_dest)
        def replace_line(prefix, replacement):
            nonlocal script
            lines = script.splitlines()
            replaced = False
            out_lines = []
            for item in lines:
                if (not replaced) and item.startswith(prefix):
                    out_lines.append(replacement)
                    replaced = True
                else:
                    out_lines.append(item)
            if not replaced:
                raise RuntimeError('missing-line:%s' % prefix)
            script = chr(10).join(out_lines) + chr(10)
        replace_line('#define SourceDir ', '#define SourceDir "%s"' % payload_dir)
        replace_line('#define OutputDir ', '#define OutputDir "%s"' % dirs[output_key])
        replace_line('SetupIconFile=', 'SetupIconFile=%s' % icon_dest)
        replace_line('OutputBaseFilename=', 'OutputBaseFilename=%s' % output_basename)

        source_lines = '\n'.join(item for item in script.splitlines() if item.lstrip().startswith('Source:'))
        self.assertIn('function CreateFileW', script)
        self.assertIn('IsTrustedLegacyUninstaller', script)
        self.assertIn('SigntoolLeafSubject', script)
        self.assertNotIn('HarkenSearch.exe', source_lines)
        self.assertNotIn('signtool.exe', source_lines)
        self.assertNotIn('unins000.exe', source_lines)
        return script, mapping

    def _write_static_receipt(self, mapping, extra):
        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        created = []
        for root, _, files in os.walk(self.run_dir):
            for name in files:
                created.append(str(Path(root) / name))
                self.assertNotIn(name, FORBIDDEN_BASENAMES)
                ok, resolved, reason = _inspect_path(Path(root) / name, must_exist=True)
                self.assertTrue(ok, '%s %s' % (name, reason))
        receipt = {
            'approvedRoot': str(APPROVED_ROOT),
            'runDir': str(self.run_dir),
            'iscc': None if self.iscc is None else str(self.iscc),
            'createdFiles': created,
            'sourceRewrites': mapping,
            'extra': extra,
            'writesProductInstallerHome': any(str(SOURCE_ROOT / 'installer') in item for item in created),
            'willExecuteSetup': False,
            'willExecuteSignTool': False,
        }
        path = LOGS_DIR / 'iss-compiles-static-safety.json'
        path.write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        self.assertFalse(receipt['writesProductInstallerHome'])
        return path

    def _compile(self, script, output_dir, output_basename, extra_defines, stem):
        if self.iscc is None:
            raise AssertionError('Inno Setup compiler was not found; the installer script was not compiled')
        script_path = self.run_dir / ('fixture_script_%s.iss' % stem)
        _write_bytes(script_path, script.encode('utf-8'))
        temp_dir = self.run_dir / 'compiler-temp'
        child_env = os.environ.copy()
        child_env['TEMP'] = str(temp_dir)
        child_env['TMP'] = str(temp_dir)
        command = [str(self.iscc), '/O%s' % output_dir] + extra_defines + [str(script_path)]
        result = subprocess.run(
            command,
            cwd=str(self.run_dir),
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            check=False,
            env=child_env,
        )
        record = {
            'command': command,
            'cwd': str(self.run_dir),
            'returncode': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'temp': str(temp_dir),
            'outputDir': str(output_dir),
            'outputBasename': output_basename,
        }
        (LOGS_DIR / ('iss-compile-%s.json' % stem)).write_text(json.dumps(record, indent=2), encoding='utf-8')
        (LOGS_DIR / ('iss-compile-%s.stdout.txt' % stem)).write_text(result.stdout or '', encoding='utf-8')
        (LOGS_DIR / ('iss-compile-%s.stderr.txt' % stem)).write_text(result.stderr or '', encoding='utf-8')
        self.assertEqual(result.returncode, 0, 'ISCC failed %s\n%s\n%s' % (stem, (result.stdout or '')[-4000:], (result.stderr or '')[-2000:]))
        # 缺失译文会静默回退英文，编译成功也必须拦截。
        self.assertNotIn('not defined for the', result.stdout + result.stderr)
        generated = list(Path(output_dir).glob('*.exe'))
        self.assertTrue(generated, 'no fixture setup produced')
        for item in generated:
            self.assertTrue(item.name.startswith('fixture_'))
            self.assertNotIn(item.name, FORBIDDEN_BASENAMES)
        return result

    def test_script_compiles_in_plain_build(self) -> None:
        dirs = self._ensure_dirs()
        script, mapping = self._rewrite_script(dirs, 'output-plain', 'fixture_setup_plain')
        self._write_static_receipt(mapping, {'branch': 'plain'})
        self._compile(script, dirs['output-plain'], 'fixture_setup_plain', [], 'plain')

    def test_script_compiles_with_signing_verifier(self) -> None:
        dirs = self._ensure_dirs()
        script, mapping = self._rewrite_script(dirs, 'output-conditional', 'fixture_setup_conditional')
        verifier = dirs['tool'] / 'fixture_verifier_payload.bin'
        _write_bytes(verifier, b'MZ-fixture-verifier')
        self._write_static_receipt(mapping, {'branch': 'conditional', 'verifier': str(verifier)})
        self._compile(
            script,
            dirs['output-conditional'],
            'fixture_setup_conditional',
            ['/DSignToolPath=%s' % verifier, '/DTrustedSignerSubject=Harken Search Fixture'],
            'conditional',
        )

    def tearDown(self) -> None:
        if not hasattr(self, 'run_dir') or not self.run_dir.exists():
            return
        ok, resolved, reason = _inspect_path(self.run_dir, must_exist=True)
        if not ok:
            (LOGS_DIR / 'cleanup-rejected.txt').write_text('refused %s %s' % (self.run_dir, reason), encoding='utf-8')
            return
        for root, _, files in os.walk(self.run_dir):
            for name in files:
                if name in FORBIDDEN_BASENAMES:
                    (LOGS_DIR / 'cleanup-rejected.txt').write_text('refused forbidden %s' % name, encoding='utf-8')
                    return
        shutil.rmtree(self.run_dir)


if __name__ == '__main__':
    unittest.main()
