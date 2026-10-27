"""离线手册必须能检索说明正文，并带有可跟做的新手路线。"""
import importlib.util
import json
from pathlib import Path

import pymupdf
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("learning_manual", ROOT / "tools/build_offline_manual.py")
manual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manual)


@pytest.fixture(scope="module")
def handbook(tmp_path_factory):
    target = tmp_path_factory.mktemp("handbook") / "manual.pdf"
    manual.generate(manual.SOURCE, target)
    return target


def test_every_illustrated_page_has_searchable_body_in_place(handbook):
    with pymupdf.open(handbook) as doc:
        by_title = {entry[1]: entry[2] - 1 for entry in doc.get_toc()}
        folder = doc[by_title["筛选器 · 文件夹"]]
        assert folder.search_for("刷新"), "已确认图解正文仍无法搜索"
        assert folder.search_for("定位")
        assert "折叠目录" in folder.get_text()
        assert all(rect.x0 > folder.rect.width / 2 for rect in folder.search_for("刷新"))
        feature_pages, _, _ = manual.load_sources(manual.SOURCE)
        for item in feature_pages:
            page = doc[by_title[item["title"]]]
            assert len(page.get_text().strip()) > 70, item["title"]
            extracted = "".join(page.get_text().split())
            for span in item["textLayer"]["spans"]:
                assert "".join(span["text"].split()) in extracted, (item["id"], span["text"])
            with pymupdf.open() as reference:
                image_page = reference.new_page(width=page.rect.width, height=page.rect.height)
                image_page.insert_image(image_page.rect, filename=str(item["path"]))
                # 看得见的图必须逐像素一致，搜索文字不得叠出重影。
                # 两边同样写入并重开 PDF，避免透明图在首次序列化时的预乘取整差。
                with pymupdf.open(stream=reference.tobytes(garbage=4, deflate=True), filetype="pdf") as saved:
                    assert page.get_pixmap(matrix=pymupdf.Matrix(.5, .5)).samples == saved[0].get_pixmap(matrix=pymupdf.Matrix(.5, .5)).samples, item["id"]


def test_learning_route_and_problem_lookup_have_valid_destinations(handbook):
    with pymupdf.open(handbook) as doc:
        by_title = {entry[1]: entry[2] - 1 for entry in doc.get_toc()}
        for title in ["第一次使用 · 添加音效", "第一次使用 · 查找与试听",
                      "第一次使用 · 整理与使用", "文件与资料库的关系",
                      "常见问题 · 查找与文件", "常见问题 · 试听与使用", "关键词索引"]:
            assert title in by_title, title
        for page in doc:
            for link in page.get_links():
                assert link["kind"] == pymupdf.LINK_GOTO
                assert 0 <= link["page"] < len(doc)
        index = doc[by_title["关键词索引"]]
        assert index.get_links()
        assert "Ctrl+F" in doc[0].get_text()
        learning = json.loads(manual.LEARNING.read_text(encoding="utf-8"))
        illustrated, _, _ = manual.load_sources(manual.SOURCE)
        title_by_id = {p["id"]: p["title"] for p in illustrated + learning["frontPages"] + learning["backPages"]}
        title_by_id["keyword-index"] = "关键词索引"
        for item in learning["frontPages"] + learning["backPages"]:
            page = doc[by_title[item["title"]]]
            extracted = "".join(page.get_text().split())
            for block in item["blocks"]:
                for value in block.values():
                    assert "".join(value.split()) in extracted
            for ref, jump in zip(item["links"], page.get_links(), strict=True):
                assert jump["page"] == by_title[title_by_id[ref["target"]]]
        for item, jump in zip(learning["index"], index.get_links(), strict=True):
            assert jump["page"] == by_title[title_by_id[item["target"]]]


@pytest.mark.parametrize("source_name", ["LEARNING", "SEARCH_TEXT"])
def test_new_copy_or_transcript_invalidates_old_pdf(tmp_path, monkeypatch, source_name):
    source = getattr(manual, source_name)
    local = tmp_path / source.name
    local.write_bytes(source.read_bytes())
    monkeypatch.setattr(manual, source_name, local)
    target = tmp_path / "manual.pdf"
    manual.generate(manual.SOURCE, target)
    data = json.loads(local.read_text(encoding="utf-8"))
    if source_name == "LEARNING":
        data["frontPages"][0]["lead"] += "新的教程说明。"
    else:
        data["pages"][0]["spans"][0]["text"] += "新"
    local.write_text(json.dumps(data), encoding="utf-8")
    with pytest.raises(ValueError, match="PDF 已过期"):
        manual.check_pdf(manual.SOURCE, target)


@pytest.mark.parametrize("change", ["wrong_image", "missing_text", "bad_link"])
def test_mismatched_text_or_broken_links_do_not_replace_pdf(tmp_path, monkeypatch, change):
    source_name = "LEARNING" if change == "bad_link" else "SEARCH_TEXT"
    source = getattr(manual, source_name)
    data = json.loads(source.read_text(encoding="utf-8"))
    if change == "wrong_image":
        data["pages"][0]["imageSha256"] = "0" * 64
    elif change == "missing_text":
        data["pages"][0]["spans"] = []
    else:
        data["frontPages"][0]["links"][0]["target"] = "missing-page"
    local = tmp_path / source.name
    local.write_text(json.dumps(data), encoding="utf-8")
    monkeypatch.setattr(manual, source_name, local)
    target = tmp_path / "manual.pdf"
    target.write_bytes(b"previous accepted PDF")
    with pytest.raises(ValueError):
        manual.generate(manual.SOURCE, target)
    assert target.read_bytes() == b"previous accepted PDF"
