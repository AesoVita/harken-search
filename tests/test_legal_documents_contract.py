from pathlib import Path
import json
import re

import pytest


ROOT = Path(__file__).resolve().parents[1]
LEGAL_STRINGS = {
    entry["id"]: entry
    for entry in json.loads(
        (ROOT / "locale" / "harken.ui.strings.json").read_text(encoding="utf-8")
    )["entries"]
}


@pytest.mark.parametrize(
    ("name", "title", "date_label", "minimum_paragraphs", "last_paragraph"),
    [
        (
            "EULA.md", "Harken Search软件安装及使用许可协议", "生效日期", 113,
            "16.5 本协议标题仅为阅读方便，不影响条款解释。",
        ),
        (
            "PRIVACY_POLICY.md", "隐私政策", "生效日期", 88,
            "如您对本政策、个人信息处理或权利行使有任何问题、意见、请求或投诉，可通过以下邮箱联系我们：harken-support@xd.com",
        ),
        (
            "OPEN_SOURCE_RELEASE_NOTICE.md", "Harken Search开源发布总说明", "发布日期", 66,
            "如本说明的任何部分被认定为无效或不可执行，不影响其他部分的效力。本说明的更新以官方仓库、官网或其他合理方式公布为准。",
        ),
    ],
)
def test_installer_legal_documents_contain_full_search_agreements(
    name, title, date_label, minimum_paragraphs, last_paragraph
):
    text = (ROOT / "docs" / "legal" / name).read_text(encoding="utf-8")
    paragraphs = [line.strip() for line in text.splitlines() if line.strip()]
    assert paragraphs[0] == title
    assert paragraphs[1] == f"{date_label}：2026年10月28日"
    assert len(paragraphs) >= minimum_paragraphs
    assert paragraphs[-1] == last_paragraph
    assert "PLACEHOLDER" not in text
    assert "待法务" not in text


def test_installer_ships_apache_license_and_requires_product_eula_page():
    text = (ROOT / "installer" / "HarkenSearch_Setup.iss").read_text(encoding="utf-8")
    assert 'Source: "{#SourceDir}\\LICENSE"; DestDir: "{app}"; Flags: ignoreversion' in text
    assert not re.search(r'^LicenseFile=', text, re.MULTILINE)
    assert "CreateCustomPage" in text
    assert "EulaPage" in text
    assert "CustomMessage('LegalConsentRequired')" in text
    assert "else if not EulaCheck.Checked then" in text


    runtime_pair = (
        'Source: "{#SourceDir}\\harken-runtime-pair.json"; '
        'DestDir: "{app}"; Flags: ignoreversion'
    )
    assert runtime_pair in text
    assert runtime_pair + " skipifsourcedoesntexist" not in text
    release_notice = (
        'Source: "{#SourceDir}\\docs\\legal\\OPEN_SOURCE_RELEASE_NOTICE.md"; '
        'DestDir: "{app}\\docs\\legal"; Flags: ignoreversion'
    )
    assert release_notice in text
    assert release_notice + " skipifsourcedoesntexist" not in text


def test_ai_disclosure_is_explicitly_pending_legal_review():
    text = (ROOT / "AI_FEATURE_DISCLOSURE.md").read_text(encoding="utf-8")
    assert "PLACEHOLDER" in text
    assert "法务占位" in text
    assert "待法务确认" in text


def _real_include_depths(source: str, header: str) -> list[int]:
    uncommented = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.DOTALL)
    include = re.compile(
        r'^\s*#\s*include\s+"' + re.escape(header) + r'"\s*$', re.MULTILINE
    )
    directive = re.compile(r"^\s*#\s*(if|ifdef|ifndef|endif)\b")
    depths: list[int] = []
    depth = 0
    for line in uncommented.splitlines():
        match = directive.match(line)
        if match:
            if match.group(1) == "endif":
                depth -= 1
            else:
                depth += 1
            continue
        if include.match(line):
            depths.append(depth)
    return depths


def _real_code_depths(source: str, pattern: str) -> list[int]:
    uncommented = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.DOTALL)
    code = re.compile(pattern)
    directive = re.compile(r"^\s*#\s*(if|ifdef|ifndef|endif)\b")
    depths: list[int] = []
    depth = 0
    for line in uncommented.splitlines():
        match = directive.match(line)
        if match:
            if match.group(1) == "endif":
                depth -= 1
            else:
                depth += 1
            continue
        if code.search(line):
            depths.append(depth)
    return depths


def test_windows_main_keeps_version_header_without_runtime_consent_dependency():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
    assert _real_include_depths(source, "Version.h") == [0]
    assert _real_include_depths(source, "ConsentManager.h") == []


def test_windows_main_fallback_keeps_shared_shutdown_state_outside_webview_guard():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
    uncommented = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.DOTALL)
    startup = uncommented.split("int WINAPI WinMain", 1)[1]

    declarations = (
        r"^\s*AnnotationBridge::Publisher\s+annotationPublisher\s*;\s*$",
        r"^\s*auto\s+logShutdownStage\s*=\s*\[\]\(const char\* stage\)\s*\{",
    )
    for declaration in declarations:
        assert _real_code_depths(startup, declaration) == [0]

    assert _real_code_depths(startup, r"annotationPublisher\.Start\(") == [1]
    assert _real_code_depths(startup, r"annotationPublisher\.Stop\(\);") == [0]
    # Both shutdown markers must also run in the build without WebView2.
    assert _real_code_depths(startup, r'logShutdownStage\("Begin"\)') == [0]
    assert _real_code_depths(startup, r'logShutdownStage\("Complete"\)') == [0]


def test_product_first_launch_does_not_require_runtime_legal_confirmation():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
    startup = source.split("int WINAPI WinMain", 1)[1]
    assert "EnsureRequiredConsent" not in startup
    assert "ResolveEarlyConsentSettingsPathUtf8" not in source
    assert "ConsentResult::" not in startup
    assert startup.index("CaptureHarkenSearchBootstrapHealthChallenge") < startup.index("RegisterOpenWith")
    executable_startup = re.sub(r"//[^\n]*", "", startup)
    assert "if (ShouldUseSingleInstanceBroker() && !bootstrapHealthSuccessor) {" in executable_startup
    assert "FindWindowW(auroWindowClassName.c_str(), nullptr)" in startup
    assert "AURO_IPC_OPEN_FILE" in startup
    assert "AURO_WM_SHOW_MANAGER" in startup
    assert "Platform::WindowSetForeground(existingWnd)" in startup


def test_single_instance_window_class_preserves_legacy_broker_identity():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
    window_class = source.split("static std::wstring GetHarkenSearchWindowClassNameW()", 1)[1].split(
        "static std::wstring GetHarkenSearchMutexNameW()", 1
    )[0]
    assert 'return L"AuroClass";' in window_class
    assert 'return L"AuroClass_" + Utf8ToWide(GetHarkenSearchInstanceKeyUtf8());' in window_class
    startup = source.split("int WINAPI WinMain", 1)[1]
    assert "GetHarkenSearchWindowClassNameW()" in startup
    assert "FindWindowW(auroWindowClassName.c_str(), nullptr)" in startup
    assert "wc.lpszClassName = auroWindowClassName.c_str();" in startup


def test_consent_dialogs_read_and_bind_the_two_packaged_documents():
    manager = (ROOT / "src" / "ConsentManager.cpp").read_text(encoding="utf-8")
    assert '"EULA.md"' in manager
    assert '"PRIVACY_POLICY.md"' in manager
    assert "ReadFileText" in manager
    assert "eulaText" in manager and "privacyText" in manager
    assert "DialogBoxIndirectParamW" in manager
    assert "WS_VSCROLL" in manager
    assert "showConsentDocument(owner, Lw(Consent_EulaCaption)" in manager
    assert "showConsentDocument(owner, Lw(Consent_PrivacyCaption)" in manager
    assert "Harken Search EULA" in LEGAL_STRINGS["Consent_EulaCaption"]["zh"]
    assert "Harken Search 隐私政策" in LEGAL_STRINGS["Consent_PrivacyCaption"]["zh"]
    assert "eulaBody" in manager and "privacyBody" in manager


def test_installer_embeds_and_displays_both_legal_documents_before_consent():
    text = (ROOT / "installer" / "HarkenSearch_Setup.iss").read_text(encoding="utf-8")
    assert "EulaFileName := LegalDocumentFileName('EULA')" in text
    assert "PrivacyFileName := LegalDocumentFileName('PRIVACY_POLICY')" in text
    assert "ExtractTemporaryFile(EulaFileName)" in text
    assert "ExtractTemporaryFile(PrivacyFileName)" in text
    assert "LoadStringsFromFile" in text
    assert "EulaMemo.Text" in text
    assert "PrivacyMemo.Text" in text
    assert "PrivacyPage" in text
    assert "PrivacyCheck.Checked" in text
    assert "else if not PrivacyCheck.Checked then" in text
    assert "LoadStringsFromFile" in text
    assert "TArrayOfString" in text
    assert "LegalText: AnsiString" not in text
    assert "GetScrollInfo" in text
    assert "SIF_RANGE or SIF_PAGE or SIF_POS" in text
    assert "ScrollInfo.nPage - 1" in text
    assert "Result := False" in text
    assert "LegalCheckClick" in text
    assert "CheckControl.OnClick := @LegalCheckClick" in text
    assert "CheckControl.Checked := False" in text
    assert "MemoControl.WordWrap := True" in text
    assert "FmtMessage(CustomMessage('LegalLoadFailed'), [CustomMessage('EulaTitle')])" in text
    assert "FmtMessage(CustomMessage('LegalLoadFailed'), [CustomMessage('PrivacyTitle')])" in text
