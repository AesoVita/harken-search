from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_windows_main_wires_annotation_bridge_only_when_explicitly_enabled():
    source = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")

    assert '#include "AnnotationBridge.h"' in source
    assert "GetEnvironmentVariableA(" in source
    assert '"ARIS_ANNOTATION_BRIDGE"' in source
    assert 'ThemeInspector::SetBridgeCollectionEnabled(annotationBridgeEnabled)' in source
    assert 'annotationPublisher.Start(' in source
    assert 'annotationPublisher.Submit(ThemeInspector::CopyLatestSnapshot())' in source
    assert 'annotationPublisher.Stop()' in source
    assert "Platform::FileSystem::TemporaryDirectory()" in source
    assert 'std::filesystem::u8path(annotationTempDirectory) / "HarkenSearchAnnotationBridge"' in source


def test_render_thread_submit_path_contains_no_file_io():
    source = (ROOT / "src" / "AnnotationBridge.cpp").read_text(encoding="utf-8")
    submit_body = source.split("void Publisher::Submit", 1)[1].split("void Publisher::Stop", 1)[0]

    assert "ofstream" not in submit_body
    assert "filesystem" not in submit_body
    assert "latestSnapshot_ = std::move(snapshot)" in submit_body
    assert "condition_.notify_one()" in submit_body


def test_publisher_reuses_the_stable_output_directory():
    source = (ROOT / "src" / "AnnotationBridge.cpp").read_text(encoding="utf-8")
    start_body = source.split("bool Publisher::Start", 1)[1].split("void Publisher::Submit", 1)[0]

    assert "EnsureDirectoryExists" in start_body
    assert "CreateDirectories" not in start_body
