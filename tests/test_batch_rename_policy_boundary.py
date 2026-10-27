from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BatchRenamePolicy.h"
SOURCE_PATH = ROOT / "src/BatchRenamePolicy.cpp"

assert HEADER_PATH.is_file()
assert SOURCE_PATH.is_file()

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
APP_STATE = (ROOT / "src/AppState.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
RULES = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
    encoding="utf-8"
)
STAGE6 = (ROOT / "tests/test_stage6_platform_boundary.py").read_text(
    encoding="utf-8"
)
STAGE7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)

for required in (
    "enum class RenameMode",
    "struct RenameRule",
    "struct RenamePreview",
    "struct PreviewInput",
    "struct PreviewLabels",
    "std::string ApplyRule(",
    "std::string ApplyRulesToFileName(",
    "std::vector<RenamePreview> BuildPreview(",
):
    assert required in HEADER

for forbidden in (
    "AppState",
    "AudioRecord",
    "Database",
    "AudioEngine",
    "UndoSystem",
    "Localization",
    "imgui",
    "windows.h",
    "WindowsMain",
    "main.cpp",
    "platform/filesystem_",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

for mode in (
    "TextReplace",
    "RegexReplace",
    "DeleteText",
    "AddPrefix",
    "AddSuffix",
    "DeleteStartChars",
    "DeleteEndChars",
    "DeleteChinese",
    "DeleteEnglish",
    "DeleteNumbers",
    "DeletePunctuation",
    "UpperToTitle",
    "TraditionalToSimplified",
    "RemoveExtraBrackets",
    "ExtractBracketContent",
    "InsertSequenceNumber",
    "ToLowercase",
    "ToUppercase",
    "ReplaceSpaces",
    "TrimSpaces",
    "DeleteFromMatchedTextToEnd",
):
    assert f"RenameMode::{mode}" in SOURCE or mode in HEADER

assert '#include "BatchRenamePolicy.h"' in APP_STATE
assert "enum class RenameMode {" not in APP_STATE
assert "struct RenameRule {" not in APP_STATE
assert "struct RenamePreview {" not in APP_STATE
for alias in (
    "using RenameMode = ::RenameMode;",
    "using RenameRule = ::RenameRule;",
    "using RenamePreview = ::RenamePreview;",
):
    assert alias in APP_STATE

assert '#include "Utils.h"' in SOURCE
assert '#include "platform/filesystem.h"' in SOURCE
assert "Platform::FileSystem::Exists" in SOURCE
assert "std::filesystem::exists" not in SOURCE
assert "fs::exists" not in SOURCE

assert "static std::string ApplyRenameRule(" not in MAIN
assert "static std::string ApplyAllRenameRules(" not in MAIN
assert "static bool IsChineseCharBR(" not in MAIN
preview_start = MAIN.index("static void GenerateBatchRenamePreview(AppState& state)")
preview_end = MAIN.index("// Execute batch rename", preview_start)
preview = MAIN[preview_start:preview_end]
assert preview.count("aura::rename::BuildPreview(") == 1
assert "state.getSelectedRecords()" in preview
assert "state.renamePreviewList =" in preview
for forbidden in (
    "std::regex",
    "newNames",
    "Platform::FileSystem::Exists",
    "ApplyRenameRule",
    "ApplyAllRenameRules",
):
    assert forbidden not in preview

assert CMAKE.count("src/BatchRenamePolicy.cpp") == 1
assert "NAME TestBatchRenamePolicy" in BEHAVIOR
assert "NAME TestBatchRenamePolicyBoundary" in BEHAVIOR
assert "tests/test_batch_rename_policy_boundary.py" in BEHAVIOR
assert '"src/BatchRenamePolicy.cpp"' in RULES
assert '"src/BatchRenamePolicy.h"' in RULES
assert "test_batch_rename_policy_owns_rule_and_preview_logic" in STAGE6
assert (
    '"src/BatchRenamePolicy.h": '
    '"tests/test_batch_rename_policy_boundary.py"'
    in STAGE7
)
