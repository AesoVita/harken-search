"""安装语言和已接入法务原稿的发布回归。实际窗口操作见同卡计划。"""
from pathlib import Path
import hashlib
import re

import pytest

ROOT = Path(__file__).resolve().parents[1]

def installer():
    return (ROOT / 'installer/HarkenSearch_Setup.iss').read_text(encoding='utf-8')

def test_installer_follows_windows_display_language_even_after_previous_install():
    text = installer()
    assert 'LanguageDetectionMethod=uilanguage' in text
    assert 'ShowLanguageDialog=no' in text
    assert 'UsePreviousLanguage=no' in text
    languages = re.findall(r'^Name: "([^"]+)"; MessagesFile: "([^"]+)"$', text, re.M)
    assert languages == [('english', 'compiler:Default.isl'),
                         ('chinesesimp', 'Languages' + chr(92) + 'ChineseSimplified.isl')]
    chinese = (ROOT / 'installer/Languages/ChineseSimplified.isl').read_text(encoding='utf-8-sig')
    assert 'LanguageID=$0804' in chinese
    assert 'SetupAppTitle=安装' in chinese

def test_first_agreement_is_product_eula_but_apache_license_is_still_shipped():
    text = installer()
    assert not re.search(r'^LicenseFile=', text, re.M)
    assert 'CreateCustomPage(wpWelcome,' in text
    assert r'Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion' in text

@pytest.mark.parametrize('name,sha,count', [
    ('EULA.en.md', 'e566c9003c93fc3c04ab2c73842263d28f8dcd3b55b3e3d34f00de3ba1ad7716', 116),
    ('PRIVACY_POLICY.en.md', 'bfe03c3a0c0824186d89a281fe299a552eba32be344374b3896e820f91256622', 67),
    ('OPEN_SOURCE_RELEASE_NOTICE.en.md', 'f123662304d8aec284fdcb2e7f1d94df1a907c1b33b22cf981de09bbf0b9c9fd', 67),
])
def test_english_documents_match_20261028_legal_original(name, sha, count):
    text = (ROOT / 'docs/legal' / name).read_text(encoding='utf-8')
    assert hashlib.sha256(text.encode()).hexdigest() == sha
    assert len([p for p in text.splitlines() if p.strip()]) == count
    assert 'October 28, 2026' in text
    assert 'PLACEHOLDER' not in text

def test_custom_messages_have_both_languages_and_no_untranslated_references():
    text = installer()
    section = text.split('[CustomMessages]', 1)[1].split('[Code]', 1)[0]
    pairs = re.findall(r'^(english|chinesesimp).([A-Za-z0-9_]+)=(.+)$', section, re.M)
    messages = {language: {key: value for lang, key, value in pairs if lang == language}
                for language in ('english', 'chinesesimp')}
    assert messages['english'].keys() == messages['chinesesimp'].keys()
    references = set(re.findall(r"CustomMessage\('([^']+)'\)", text))
    assert references and references <= messages['english'].keys()
    for key, value in messages['english'].items():
        assert not re.search('[\u4e00-\u9fff]', value), key
        assert re.findall(r'%[1-9n]', value) == re.findall(r'%[1-9n]', messages['chinesesimp'][key]), key

def test_all_six_legal_documents_are_required_by_every_packaging_path():
    text = installer()
    qa = (ROOT / 'scripts/package_qa_exe.ps1').read_text(encoding='utf-8')
    workflow = (ROOT / '.github/workflows/package-update.yml').read_text(encoding='utf-8')
    for base in ('EULA', 'PRIVACY_POLICY', 'OPEN_SOURCE_RELEASE_NOTICE'):
        for suffix in ('.md', '.en.md'):
            name = base + suffix
            expected = rf'Source: "{{#SourceDir}}\docs\legal\{name}"; DestDir: "{{app}}\docs\legal"; Flags: ignoreversion'
            assert expected in text
            assert expected + ' skipifsourcedoesntexist' not in text
            assert rf'"docs\legal\{name}"' in qa
            assert workflow.count(f"'{name}'") == 2
