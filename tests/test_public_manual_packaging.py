"""Public packages use the committed PDF and its generation receipt."""
import json
import shutil

import pytest

from tools import build_offline_manual as manual


def public_manual(tmp_path, monkeypatch):
    output = tmp_path / "manual.pdf"
    shutil.copyfile(manual.DEFAULT_PDF, output)
    shutil.copyfile(manual.DEFAULT_PDF.with_suffix(".json"), output.with_suffix(".json"))
    source = tmp_path / "unpublished-source"
    monkeypatch.setattr(manual, "SOURCE", source)
    return source, output


def test_public_checkout_stages_the_committed_manual(tmp_path, monkeypatch):
    source, output = public_manual(tmp_path, monkeypatch)
    staged = manual.stage_manual(tmp_path / "package", source=source, output=output)
    assert staged.read_bytes() == output.read_bytes()


@pytest.mark.parametrize("change", ["pdf", "receipt", "schema"])
def test_invalid_public_manual_never_replaces_a_staged_copy(tmp_path, monkeypatch, change):
    source, output = public_manual(tmp_path, monkeypatch)
    package = tmp_path / "package"
    existing = package / manual.RELATIVE_PDF
    existing.parent.mkdir(parents=True)
    existing.write_bytes(b"keep existing manual")
    receipt = output.with_suffix(".json")
    if change == "pdf":
        output.write_bytes(b"%PDF-changed")
    elif change == "receipt":
        receipt.unlink()
    else:
        record = json.loads(receipt.read_text(encoding="utf-8"))
        record["schemaVersion"] = 999
        receipt.write_text(json.dumps(record), encoding="utf-8")
    with pytest.raises((ValueError, OSError)):
        manual.stage_manual(package, source=source, output=output)
    assert existing.read_bytes() == b"keep existing manual"


def test_partial_source_tree_is_not_treated_as_public_receipt_only(tmp_path, monkeypatch):
    source, output = public_manual(tmp_path, monkeypatch)
    source.mkdir()
    with pytest.raises(ValueError, match="资料文件缺失"):
        manual.stage_manual(tmp_path / "package", source=source, output=output)


def test_misspelled_explicit_source_is_rejected(tmp_path, monkeypatch):
    _, output = public_manual(tmp_path, monkeypatch)
    with pytest.raises(ValueError, match="资料文件缺失"):
        manual.check_pdf(tmp_path / "wrong-source", output)
