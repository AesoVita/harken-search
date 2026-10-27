"""离线手册通过真实 PDF 及隔离打包目录验收。"""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

import pymupdf
import pytest
from PIL import Image, ImageChops
from tools import build_offline_manual as manual

ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "tools/build_offline_manual.py"
SOURCE = ROOT / "documents/harken-search-manual-confirmed"
RELATIVE_PDF = Path("docs/HarkenSearch-Manual-zh-CN.pdf")


def run_builder(*args):
    return subprocess.run([sys.executable, str(BUILDER), *map(str, args)],
                          capture_output=True, text=True, encoding="utf-8")


def test_real_manual_has_every_image_clickable_contents_and_bookmarks(tmp_path):
    target = tmp_path / "manual.pdf"
    result = run_builder("--output", target)
    assert result.returncode == 0, result.stderr
    pages = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))["pages"]
    settings = json.loads((SOURCE / "settings-simple/manifest.json").read_text(encoding="utf-8"))["pages"]
    images = [SOURCE / p["image"] for p in pages]
    images += [SOURCE / "settings-simple" / p["image"]["path"] for p in settings]
    with pymupdf.open(target) as pdf:
        assert len(pdf) == 44
        assert "目录" in pdf[1].get_text()
        assert "软件简介" in pdf[1].get_text()
        assert "总界面导览" in pdf[1].get_text()
        assert sorted(link["page"] for toc in (pdf[1], pdf[2]) for link in toc.get_links()) == list(range(3, 44))
        assert [entry[2] for entry in pdf.get_toc() if entry[0] == 2] == list(range(4, 45))
        assert "本地音效管理" in pdf[3].get_text()
        for title in ["搜索与模式切换", "筛选器", "搜索结果", "音效信息", "相似音效", "波形与试听"]:
            assert title in pdf[4].get_text()
        screenshot = pymupdf.Pixmap(ROOT / "docs/manual/ui-overview.png")
        overview_images = pdf[4].get_images()
        assert len(overview_images) == 2  # Logo + 脱敏截图，不暗藏原图。
        embedded_overview = next(im for im in overview_images if (im[2], im[3]) == (screenshot.width, screenshot.height))
        assert pymupdf.Pixmap(pdf, embedded_overview[0]).samples == screenshot.samples
        assert "示例" in pdf[4].get_text()
        for page, source in zip(list(pdf)[9:41], images):
            original = pymupdf.Pixmap(source)
            image = page.get_images()[0]
            embedded = pymupdf.Pixmap(pdf, image[0])
            if image[1]:
                embedded = pymupdf.Pixmap(embedded, pymupdf.Pixmap(pdf, image[1]))
            elif original.alpha:
                embedded = pymupdf.Pixmap(embedded, True)
            assert (embedded.width, embedded.height) == (original.width, original.height)
            mode = "RGBA" if original.alpha else "RGB"
            # PDF 的透明蒙版重组会出现至多 1/255 的预乘取整差；不得重采样或 JPEG 压缩。
            difference = ImageChops.difference(
                Image.frombytes(mode, (original.width, original.height), original.samples),
                Image.frombytes(mode, (embedded.width, embedded.height), embedded.samples),
            ).getextrema()
            assert max(high for low, high in difference[:3]) <= 1
            if original.alpha:
                assert difference[3] == (0, 0)
            assert abs(page.rect.width / page.rect.height - original.width / original.height) < 0.001


@pytest.mark.parametrize("change", ["missing", "changed", "escape"])
def test_invalid_source_does_not_replace_existing_pdf(tmp_path, change):
    source = tmp_path / "source"
    shutil.copytree(SOURCE, source)
    image = source / "filter-folder.png"
    if change == "missing":
        image.unlink()
    elif change == "changed":
        image.write_bytes(b"not the approved image")
    else:
        manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
        manifest["pages"][0]["image"] = "../outside.png"
        (source / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    target = tmp_path / "keep.pdf"
    target.write_bytes(b"keep this existing file")
    result = run_builder("--source-dir", source, "--output", target)
    assert result.returncode != 0
    expected = {"missing": "资料文件缺失", "changed": "图片与确认清单不一致", "escape": "资料路径超出目录"}
    assert expected[change] in result.stderr
    assert target.read_bytes() == b"keep this existing file"


def test_staging_copies_verified_pdf_and_rejects_modified_pdf(tmp_path):
    target = tmp_path / "manual.pdf"
    assert run_builder("--output", target).returncode == 0
    stage = tmp_path / "package"
    result = run_builder("--output", target, "--check", "--stage-dir", stage)
    assert result.returncode == 0, result.stderr
    assert (stage / RELATIVE_PDF).read_bytes() == target.read_bytes()
    before = hashlib.sha256((stage / RELATIVE_PDF).read_bytes()).hexdigest()
    target.write_bytes(b"broken pdf")
    result = run_builder("--output", target, "--check", "--stage-dir", stage)
    assert result.returncode != 0
    assert hashlib.sha256((stage / RELATIVE_PDF).read_bytes()).hexdigest() == before


def test_update_manual_uses_a_directory_present_in_public_1038(tmp_path):
    # The public 1.0.38 bootstrap can create files, but Begin rejects a file
    # whose parent directory is absent. Its install has docs/legal, not
    # docs/manual: introducing that directory made the real update return 30.
    stage = tmp_path / "payload"
    (stage / "docs/legal").mkdir(parents=True)
    existing_directories = {path.relative_to(stage) for path in stage.rglob("*")
                            if path.is_dir()} | {Path(".")}
    target = manual.stage_manual(stage)
    assert target.relative_to(stage).parent in existing_directories
    assert target.read_bytes() == manual.DEFAULT_PDF.read_bytes()


def test_staging_replaces_the_old_packaged_manual_without_removing_other_docs(tmp_path):
    stage = tmp_path / "payload"
    legacy = stage / "docs/manual/HarkenSearch-Manual-zh-CN.pdf"
    legacy.parent.mkdir(parents=True)
    legacy.write_bytes(b"previous staged manual")
    unrelated = legacy.parent / "keep.txt"
    unrelated.write_bytes(b"keep unrelated documentation")
    target = manual.stage_manual(stage)
    assert target.read_bytes() == manual.DEFAULT_PDF.read_bytes()
    assert not legacy.exists()
    assert unrelated.read_bytes() == b"keep unrelated documentation"
    assert manual.DEFAULT_PDF.is_file()


def test_changed_contents_invalidates_old_pdf(tmp_path):
    source = tmp_path / "source"
    shutil.copytree(SOURCE, source)
    target = tmp_path / "manual.pdf"
    assert run_builder("--source-dir", source, "--output", target).returncode == 0
    manifest_path = source / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["pages"][0]["title"] = "已更新的目录标题"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    result = run_builder("--source-dir", source, "--output", target, "--check")
    assert result.returncode != 0
    assert "PDF 已过期" in result.stderr


@pytest.mark.parametrize("change", ["text", "image", "missing"])
def test_intro_and_overview_changes_cannot_leave_stale_or_incomplete_pdf(tmp_path, monkeypatch, change):
    intro_file = tmp_path / "introduction.json"
    shutil.copy2(ROOT / "docs/manual/introduction.json", intro_file)
    image = tmp_path / "ui-overview.png"
    shutil.copy2(ROOT / "docs/manual/ui-overview.png", image)
    monkeypatch.setattr(manual, "INTRODUCTION", intro_file)
    target = tmp_path / "manual.pdf"
    manual.generate(SOURCE, target)
    previous = target.read_bytes()
    if change == "text":
        introduction = json.loads(intro_file.read_text(encoding="utf-8"))
        introduction["lead"] += "这是新介绍。"
        intro_file.write_text(json.dumps(introduction), encoding="utf-8")
        with pytest.raises(ValueError, match="PDF 已过期"):
            manual.check_pdf(SOURCE, target)
    else:
        if change == "image":
            image.write_bytes(b"unexpected screenshot")
            message = "总界面图片与资料清单不一致"
        else:
            image.unlink()
            message = "资料文件缺失"
        with pytest.raises(ValueError, match=message):
            manual.generate(SOURCE, target)
    assert target.read_bytes() == previous
