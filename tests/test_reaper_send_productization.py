from pathlib import Path
import json
import re


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    path = ROOT / relative
    assert path.is_file(), f"missing required REAPER productization file: {relative}"
    return path.read_text(encoding="utf-8")


def require(source: str, needle: str, message: str) -> None:
    assert needle in source, message


def test_reaper_productization_contract() -> None:
    version = read("src/ReaperBridgeVersion.h")
    protocol = read("src/ReaperBridgeProtocol.h")
    status = read("src/ReaperBridgeStatus.h")
    installer_h = read("src/DawBridgeInstaller.h")
    installer_cpp = read("src/DawBridgeInstaller.cpp")
    doctor_cpp = read("src/ReaperBridgeDoctor.cpp")
    file_hash_h = read("src/platform/file_hash.h")
    file_hash_win32 = read("src/platform/file_hash_win32.cpp")
    client = read("src/ReaperBridgeClient.cpp")
    bridge = read("reaper_extension/reaper_harken.cpp")
    bridge_rc = read("reaper_extension/reaper_harken.rc")
    main = read("src/main.cpp")
    installer = read("installer/HarkenSearch_Setup.iss")
    real_harness = read("tools/reaper_real_validation.ps1")

    protocol_match = re.search(
        r"ARIS_REAPER_BRIDGE_PROTOCOL_VERSION\s+(\d+)", version
    )
    assert protocol_match and int(protocol_match.group(1)) >= 3, (
        "REAPER bridge protocol must be v3+ for one-request batch placement"
    )

    for needle in (
        "BuildInsertBatchRequest",
        "ParseInsertBatchRequest",
        "EvaluatePlacement",
        "BuildInsertBatchSuccessResponse",
    ):
        require(protocol, needle, f"protocol is missing {needle}")

    require(status, "WaitingForHostExit", "state model must expose running-host wait")
    require(installer_h, "configuredReaperRootPath", "Doctor must accept a REAPER root")
    require(installer_h, "sha256", "binary probe must expose SHA-256")
    require(installer_h, "protocolVersion", "binary probe must expose offline protocol")
    require(file_hash_h, "ComputeSha256File", "SHA-256 must have a platform crypto PAL")
    require(file_hash_win32, "BCRYPT_SHA256_ALGORITHM", "Windows hash PAL must use CNG SHA-256")
    require(doctor_cpp, "Platform::Crypto::ComputeSha256File", "Doctor must use the hash PAL")
    require(doctor_cpp, "BridgeProtocolVersion", "Doctor must inspect offline protocol metadata")
    require(doctor_cpp, "BridgeManifest", "Doctor must inspect a trusted manifest marker")
    require(doctor_cpp, "LegacyReaperPluginFileName", "upgrade must locate the previous REAPER plugin name")
    require(doctor_cpp, "retireLegacyPlugin", "upgrade must retire a confirmed old plugin beside the new one")
    require(installer_cpp, "ProbeReaperBridge", "registry must use the shared Doctor probe")
    require(client, "BuildInsertBatchRequest", "client must send one batch request")
    require(bridge, "GetCursorPosition", "extension must snapshot the edit cursor")
    require(bridge, "SetEditCurPos", "extension must control monotonic cursor placement")
    require(bridge, "GetMediaItemInfo_Value", "extension must verify actual inserted item spans")
    require(bridge, "DeleteTrackMediaItem", "extension must remove batch items on failure")
    require(bridge, "ParseInsertBatchRequest", "extension must consume the v3 batch request")
    require(bridge_rc, "BridgeProtocolVersion", "DLL resource must publish protocol metadata")
    require(bridge_rc, "BridgeManifest", "DLL resource must publish a manifest marker")

    strings = {
        entry["id"]: entry["zh"]
        for entry in json.loads(read("locale/harken.ui.strings.json"))["entries"]
    }
    for key, chinese_label in (
        ("Main_ReaperRootDir", "REAPER 根目录"),
        ("Main_AutoDetect", "自动检测"),
        ("Main_InstallReaperBridge", "安装/更新 REAPER 发送组件"),
        ("Main_RecheckBridge", "重新检测"),
        ("Main_CopyDiagnostic", "复制脱敏诊断摘要"),
    ):
        require(main, f"L({key})", f"settings UI is missing: {key}")
        assert chinese_label in strings[key], f"localized settings label changed: {key}"

    required_bridge_line = (
        'Source: "{#SourceDir}\\reaper_harken.dll"; DestDir: "{app}"; '
        "Flags: ignoreversion"
    )
    require(installer, required_bridge_line, "formal installer must require reaper_harken.dll")
    require(real_harness, "--doctor-update", "real harness must use the shared Doctor")
    require(real_harness, "-close:save:exit", "real harness must close REAPER gracefully")
    require(real_harness, "HARNESS-REAPER-REAL PASS", "real harness must emit durable proof")
    assert "Stop-Process" not in real_harness, "real harness must never force-kill REAPER"


if __name__ == "__main__":
    test_reaper_productization_contract()
    print("PASS: REAPER send productization source contract")
