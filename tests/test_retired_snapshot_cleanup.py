import importlib.util
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


def module():
    spec = importlib.util.spec_from_file_location("cleanup", ROOT / "tools/retired_snapshot_cleanup.py")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def fixture(tmp_path):
    tool = module()
    root = tmp_path / "repo"
    root.mkdir()
    (root / ".git").mkdir()
    snapshot = root / ".local" / tool.SNAPSHOTS[0]
    snapshot.mkdir(parents=True)
    (snapshot / "keep-proof.txt").write_bytes(b"historical evidence")
    return tool, root, snapshot


def test_plan_is_read_only_and_apply_verifies_archive(tmp_path):
    tool, root, snapshot = fixture(tmp_path)
    plan = tool.make_plan(root)
    assert snapshot.exists()
    receipt = tool.apply_plan(root, plan, tmp_path / "archive.zip")
    assert receipt["removed"] == [str(snapshot)]
    assert not snapshot.exists()
    import zipfile
    with zipfile.ZipFile(tmp_path / "archive.zip") as archive:
        assert archive.read(snapshot.name + "/keep-proof.txt") == b"historical evidence"


def test_changed_source_prevents_any_deletion(tmp_path):
    tool, root, snapshot = fixture(tmp_path)
    plan = tool.make_plan(root)
    (snapshot / "keep-proof.txt").write_bytes(b"changed after review")
    with pytest.raises(ValueError, match="changed"):
        tool.apply_plan(root, plan, tmp_path / "archive.zip")
    assert snapshot.exists()


def test_modified_target_outside_allowlist_is_rejected(tmp_path):
    tool, root, snapshot = fixture(tmp_path)
    plan = tool.make_plan(root)
    plan["snapshots"][0]["name"] = "../../other"
    with pytest.raises(ValueError):
        tool.apply_plan(root, plan, tmp_path / "archive.zip")
    assert snapshot.exists()


def test_actual_repository_copy_is_not_treated_as_export(tmp_path):
    tool, root, snapshot = fixture(tmp_path)
    (snapshot / ".git").write_text("gitdir: elsewhere", encoding="utf-8")
    with pytest.raises(ValueError, match="repository"):
        tool.make_plan(root)
