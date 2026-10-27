"""从正式图解清单生成 PDF；打包校验与复制仅使用 Python 标准库。"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "documents/harken-search-manual-confirmed"
LOGO = ROOT / "assets/logos/brand/harken-search-wordmark-orange-gradient.png"
# Released bootstraps require payload parents to exist in the old installation.
# Keep source material in docs/manual, but ship the PDF in the existing docs root.
RELATIVE_PDF = Path("docs/HarkenSearch-Manual-zh-CN.pdf")
DEFAULT_PDF = ROOT / "docs/manual/HarkenSearch-Manual-zh-CN.pdf"
INTRODUCTION = ROOT / "docs/manual/introduction.json"
LEARNING = ROOT / "docs/manual/learning.json"
SEARCH_TEXT = ROOT / "docs/manual/search-text.json"
RECIPE_VERSION = 3


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def local_file(root: Path, relative: str) -> Path:
    candidate = (root / relative).resolve()
    if not candidate.is_relative_to(root.resolve()):
        raise ValueError(f"资料路径超出目录：{relative}")
    if not candidate.is_file():
        raise ValueError(f"资料文件缺失：{relative}")
    return candidate


def load_sources(source: Path) -> tuple[list[dict], dict, str]:
    manifest_path = local_file(source, "manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    settings_path = local_file(source, manifest["settingsSuite"]["manifest"])
    settings = json.loads(settings_path.read_text(encoding="utf-8-sig"))
    if not manifest.get("official") or not settings.get("official"):
        raise ValueError("只能使用已确认的正式说明图")
    rows = {r["source_line"]: r["name"] for r in settings["sourceRowMapping"]}
    pages = []
    for p in manifest["pages"]:
        if not p.get("official"):
            raise ValueError(f"功能图尚未确认：{p['id']}")
        pages.append(dict(id=p["id"], title=p["title"], group="功能图解",
                          path=local_file(source, p["image"]), sha256=p["imageSha256"]))
    if settings["pageCount"] != len(settings["pages"]):
        raise ValueError("设置说明页数与清单不一致")
    for p in settings["pages"]:
        if p.get("status") != "official":
            raise ValueError(f"设置图尚未确认：{p['pageId']}")
        first = re.split(r"[（(]", rows[p["sourceLines"][0]])[0].strip()
        pages.append(dict(id=p["pageId"], title=f"{p['moduleTitle']} · {first}", group="设置说明",
                          path=local_file(settings_path.parent, p["image"]["path"]),
                          sha256=p["image"]["sha256"]))
    if not pages:
        raise ValueError("正式说明图清单为空")
    for p in pages:
        if digest(p["path"]) != p["sha256"]:
            raise ValueError(f"图片与确认清单不一致：{p['path'].name}")
    introduction = json.loads(INTRODUCTION.read_text(encoding="utf-8-sig"))
    overview = introduction["overview"]
    overview_path = local_file(INTRODUCTION.parent, overview["image"])
    if digest(overview_path) != overview["sha256"]:
        raise ValueError("总界面图片与资料清单不一致")
    learning = json.loads(LEARNING.read_text(encoding="utf-8-sig"))
    search_text = json.loads(SEARCH_TEXT.read_text(encoding="utf-8-sig"))
    text_pages = {p["id"]: p for p in search_text["pages"]}
    if len(text_pages) != len(search_text["pages"]) or set(text_pages) != {p["id"] for p in pages}:
        raise ValueError("搜索文字层与正式图解页不对应")
    for p in pages:
        layer = text_pages[p["id"]]
        if layer["imageSha256"] != p["sha256"]:
            raise ValueError(f"搜索文字层不属于当前图片：{p['id']}")
        if not layer["spans"]:
            raise ValueError(f"图解没有搜索正文：{p['id']}")
        for span in layer["spans"]:
            if not (span["text"].strip() and span["width"] > 0 and span["size"] > 0
                    and 0 <= span["x"] < span["x"] + span["width"] <= layer["width"]
                    and 0 <= span["y"] < span["y"] + span["size"] <= layer["height"]):
                raise ValueError(f"搜索正文超出图片或为空：{p['id']}")
        p["textLayer"] = layer
    identity = {"recipe": RECIPE_VERSION, "manifest": manifest,
                "settings": settings, "logo": digest(LOGO),
                "introduction": introduction, "learning": learning, "searchText": search_text,
                "images": [p["sha256"] for p in pages]}
    signature = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    overview["resolvedPath"] = overview_path
    introduction["learning"] = learning
    return pages, introduction, signature


def check_pdf(source: Path, output: Path) -> dict:
    receipt = json.loads(output.with_suffix(".json").read_text(encoding="utf-8"))
    if (not isinstance(receipt, dict) or receipt.get("schemaVersion") != RECIPE_VERSION
            or not re.fullmatch(r"[0-9a-f]{64}", str(receipt.get("sourceSignature", "")))
            or not re.fullmatch(r"[0-9a-f]{64}", str(receipt.get("pdfSha256", "")))
            or type(receipt.get("imageCount")) is not int or receipt["imageCount"] <= 0
            or type(receipt.get("pageCount")) is not int
            or receipt["pageCount"] < receipt["imageCount"]):
        raise ValueError("PDF 生成记录无效，请重新生成")
    if digest(output) != receipt["pdfSha256"] or not output.read_bytes().startswith(b"%PDF-"):
        raise ValueError("PDF 内容与生成记录不一致，请重新生成")
    # The public checkout ships the reviewed PDF and receipt, not the private
    # illustration workspace. A present (even incomplete) source directory and
    # any explicit non-default source still require full source validation.
    if source.resolve() != SOURCE.resolve() or source.exists():
        pages, _, signature = load_sources(source)
        if receipt["sourceSignature"] != signature or receipt["imageCount"] != len(pages):
            raise ValueError("PDF 已过期，请从当前正式图解重新生成")
    return receipt


def stage_manual(destination: str | Path, *, source: Path = SOURCE, output: Path = DEFAULT_PDF) -> Path:
    check_pdf(source, output)
    target = Path(destination) / RELATIVE_PDF
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.resolve() != output.resolve():
        shutil.copyfile(output, target)
    if digest(target) != digest(output):
        raise ValueError("打包目录中的 PDF 与生成文件不一致")
    # An incremental build may still contain the previous staged copy. The
    # release handoff recursively copies docs, so leaving it would reintroduce
    # the missing-parent failure even though the new PDF is in the right place.
    legacy = Path(destination) / "docs/manual/HarkenSearch-Manual-zh-CN.pdf"
    if legacy.resolve() != output.resolve() and legacy.is_file():
        legacy.unlink()
        try:
            legacy.parent.rmdir()  # Remove only an empty generated directory.
        except OSError:
            pass
    return target


def generate(source: Path, output: Path) -> dict:
    # 先检查所有输入，避免用缺页的产物覆盖已有手册。
    pages, introduction, signature = load_sources(source)
    learning = introduction["learning"]
    import pymupdf as pdf

    doc = pdf.open()
    orange, ink, muted = (0.769, 0.318, 0.047), (0.114, 0.141, 0.169), (0.376, 0.408, 0.443)
    background = (0.973, 0.976, 0.980)
    font = pdf.Font("china-s")
    fallback_font = pdf.Font("helv")

    def text(page, x, y, value, size=12, color=ink):
        page.insert_text((x, y), value, fontsize=size, fontname="ManualFont", color=color)

    def paper():
        page = doc.new_page(width=720, height=940)
        page.draw_rect(page.rect, color=background, fill=background)
        page.insert_font(fontname="ManualFont", fontbuffer=font.buffer)
        return page

    def paragraph(page, x, y, value, width=632, height=90, size=14, color=muted):
        # 混排按真实字宽换行，避免把一长段无空格中文当作英文单词另起一行。
        lines, line = [], ""
        for character in value:
            if character == "\n":
                lines.append(line)
                line = ""
            elif line and font.text_length(line + character, fontsize=size) > width:
                if character in "，。；：！？、）》】":
                    lines.append(line[:-1].rstrip())
                    line = line[-1] + character
                else:
                    lines.append(line.rstrip())
                    line = character.lstrip()
            else:
                line += character
        if line:
            lines.append(line)
        if len(lines) * size * 1.55 > height:
            raise ValueError(f"手册文字超出预留区域：{value}")
        for row, line in enumerate(lines):
            text(page, x, y + size + row * size * 1.55, line, size, color)
        return y + len(lines) * size * 1.55

    def heading_page(title):
        page = paper()
        page.insert_image(pdf.Rect(44, 32, 280, 74), filename=str(LOGO), keep_proportion=True)
        page.draw_line((44, 96), (676, 96), color=(0.86, 0.88, 0.90))
        text(page, 44, 144, title, 26, orange)
        return page

    cover = paper()
    cover.insert_image(pdf.Rect(44, 62, 430, 120), filename=str(LOGO), keep_proportion=True)
    cover.draw_line((44, 156), (676, 156), color=(0.86, 0.88, 0.90))
    text(cover, 44, 242, "新手入门与功能手册", 32, orange)
    text(cover, 44, 288, "Harken Search 音效管理器", 18)
    text(cover, 44, 344, "跟着完成第一次使用，再按需要查阅功能与设置。", 14, muted)
    features = sum(p["group"] == "功能图解" for p in pages)
    text(cover, 44, 402, f"{features} 张功能图解 · {len(pages) - features} 张设置说明", 16)
    text(cover, 44, 436, "软件简介 · 总界面 · 上手教程 · 核心概念 · 常见问题", 14, muted)
    text(cover, 44, 650, "阅读方式", 17, orange)
    text(cover, 44, 692, "点击下一页目录，或使用 PDF 阅读器的书签跳转。", 13)
    text(cover, 44, 722, "图解保留原始清晰度，可放大查看控件与文字。", 13)
    text(cover, 44, 752, "按 Ctrl+F 搜索说明正文，也可点击最后的关键词索引。", 13)
    text(cover, 44, 850, "图中素材与封面为说明操作使用的示例。", 11, muted)
    edition = json.loads((source / "manifest.json").read_text(encoding="utf-8-sig"))["savedAt"][:10]
    text(cover, 44, 878, f"操作参考版本：{learning['appVersion']}   手册修订：{learning['revisionDate']}", 11, muted)
    text(cover, 44, 901, f"已确认图解资料：{edition}；界面可能随软件版本更新。", 10, muted)

    entries = [dict(id="introduction", title=introduction["title"], group="认识软件"),
               dict(id="overview", title=introduction["overview"]["title"], group="认识软件")]
    entries += learning["frontPages"] + pages + learning["backPages"]
    entries += [dict(id="keyword-index", title="关键词索引", group="快速查询")]
    destinations = {entry["id"]: index + 3 for index, entry in enumerate(entries)}
    if len(destinations) != len(entries):
        raise ValueError("手册内容 ID 重复")
    links, outlines = [], [[1, "目录", 2]]
    def link(page, rect, target):
        if target not in destinations:
            raise ValueError(f"手册链接目标不存在：{target}")
        links.append((page.number, rect, destinations[target]))

    layouts = [
        [("认识软件", 44, 152), ("开始使用", 44, 300), ("功能图解", 366, 152)],
        [("设置说明", 44, 152), ("遇到问题", 366, 152), ("快速查询", 366, 300)],
    ]
    for number, layout in enumerate(layouts):
        contents = paper()
        text(contents, 44, 72, "目录" if number == 0 else "目录 · 设置与问题", 28, orange)
        text(contents, 44, 103, "点击条目即可跳转；首次使用建议先读「开始使用」。", 11, muted)
        for group, x, top in layout:
            text(contents, x, top, group, 17, orange)
            group_pages = [p for p in entries if p["group"] == group]
            if group_pages:
                outlines.append([1, group, destinations[group_pages[0]["id"]] + 1])
            for row, p in enumerate(group_pages):
                y = top + 38 + row * 33
                if y > 890:
                    raise ValueError("目录超出本页，需要扩充目录页后再生成")
                label, size = p["title"], 10.5
                if font.text_length(label, fontsize=size) > 258:
                    raise ValueError(f"目录标题过长，请用可读的短标题：{label}")
                text(contents, x, y, label, size)
                text(contents, x + 278, y, str(destinations[p["id"]] + 1), size, muted)
                link(contents, pdf.Rect(x, y - 15, x + 300, y + 10), p["id"])
                outlines.append([2, label, destinations[p["id"]] + 1])

    intro_page = heading_page(introduction["title"])
    text(intro_page, 44, 210, introduction["heading"], 23)
    paragraph(intro_page, 44, 241, introduction["lead"], height=105, size=15, color=ink)
    if len(introduction["sections"]) > 3:
        raise ValueError("简介超过三段，需要调整版面")
    for row, section in enumerate(introduction["sections"]):
        y = 396 + row * 122
        text(intro_page, 44, y, section["title"], 18, orange)
        paragraph(intro_page, 44, y + 20, section["text"], height=78)
    text(intro_page, 44, 807, "从哪里开始阅读", 17, orange)
    paragraph(intro_page, 44, 829, introduction["readingTip"], height=76, size=13)

    overview = introduction["overview"]
    page = heading_page(overview["title"])
    text(page, 44, 171, overview["caption"], 11, muted)
    pix = pdf.Pixmap(overview["resolvedPath"])
    bounds = pdf.Rect(44, 198, 676, 198 + 632 * pix.height / pix.width)
    page.insert_image(bounds, filename=str(overview["resolvedPath"]), keep_proportion=True)
    sx, sy = bounds.width / overview["coordinateWidth"], bounds.height / overview["coordinateHeight"]
    text(page, 44, 578, "主要区域", 17, orange)
    for index, region in enumerate(overview["regions"]):
        if index >= 6:
            raise ValueError("总界面超过六个区域，需要调整版面")
        x0, y0, x1, y1 = region["box"]
        if not (0 <= x0 < x1 <= overview["coordinateWidth"] and
                0 <= y0 < y1 <= overview["coordinateHeight"]):
            raise ValueError("总界面标注超出截图边界")
        box = pdf.Rect(bounds.x0 + x0 * sx, bounds.y0 + y0 * sy,
                       bounds.x0 + x1 * sx, bounds.y0 + y1 * sy)
        page.draw_rect(box, color=orange, width=0.65)
        bx, by = region["badge"]
        bx, by = bounds.x0 + bx * sx, bounds.y0 + by * sy
        page.draw_rect(pdf.Rect(bx - 6, by - 6, bx + 6, by + 6), color=orange, fill=orange)
        text(page, bx - 2.5, by + 3.3, str(region["number"]), 9, (1, 1, 1))
        x, y = 44 + (index % 2) * 322, 620 + (index // 2) * 93
        page.draw_rect(pdf.Rect(x, y - 14, x + 15, y + 1), color=orange, fill=orange)
        text(page, x + 4.5, y - 2.3, str(region["number"]), 10, (1, 1, 1))
        text(page, x + 25, y, region["title"], 15)
        paragraph(page, x + 25, y + 13, region["text"], width=275, height=62, size=12)
    text(page, 44, 901, overview["exampleNote"], 10, muted)

    blue = (0.086, 0.455, 0.722)

    def learning_page(content):
        page = heading_page(content["title"])
        cursor = paragraph(page, 44, 184, content["lead"], height=104, size=14, color=ink) + 26
        if content.get("diagram"):
            labels = ["原始音效文件", "扫描建立记录", "搜索与筛选", "试听与列表组织"]
            for index, label in enumerate(labels):
                x = 44 + index * 161
                page.draw_rect(pdf.Rect(x, cursor, x + 148, cursor + 49), color=orange, width=.8)
                text(page, x + 11, cursor + 29, label, 12, ink)
                if index < 3:
                    text(page, x + 150, cursor + 29, "›", 14, orange)
            cursor += 78
        for block in content["blocks"]:
            text(page, 44, cursor, block["title"], 17, orange)
            cursor = paragraph(page, 44, cursor + 15, block["text"], height=125, size=13.3)
            if block.get("result"):
                cursor = paragraph(page, 44, cursor + 7, block["result"], height=55, size=11.5, color=blue)
            cursor += 30
        if cursor > 861:
            raise ValueError(f"教程正文过长，需要分段或分页面：{content['title']} ({cursor})")
        page.draw_line((44, 862), (676, 862), color=(.86, .88, .90))
        for index, ref in enumerate(content.get("links", [])):
            x = 44 + index * 322
            text(page, x, 889, ref["label"], 12, blue)
            link(page, pdf.Rect(x, 872, x + 310, 904), ref["target"])

    for content in learning["frontPages"]:
        learning_page(content)

    for p in pages:
        pix = pdf.Pixmap(p["path"])
        page = doc.new_page(width=720, height=720 * pix.height / pix.width)
        page.insert_image(page.rect, filename=str(p["path"]), keep_proportion=True)
        layer = p["textLayer"]
        if (pix.width, pix.height) != (layer["width"], layer["height"]):
            raise ValueError(f"文字层坐标尺寸与图片不同：{p['id']}")
        page.insert_font(fontname="ManualFont", fontbuffer=font.buffer)
        scale = 720 / pix.width
        page.insert_font(fontname="ManualFallback", fontbuffer=fallback_font.buffer)
        for span in layer["spans"]:
            size = span["size"] * scale
            runs = []
            for char in span["text"]:
                face = "ManualFont" if font.has_glyph(ord(char)) else "ManualFallback"
                if face == "ManualFallback" and not fallback_font.has_glyph(ord(char)):
                    raise ValueError(f"文字层字体缺少字符：{char}")
                if runs and runs[-1][0] == face:
                    runs[-1][1] += char
                else:
                    runs.append([face, char])
            widths = [(font if face == "ManualFont" else fallback_font).text_length(value, fontsize=size)
                      for face, value in runs]
            stretch = span["width"] * scale / sum(widths)
            x = span["x"] * scale
            for (face, value), width in zip(runs, widths):
                point = pdf.Point(x, (span["y"] + span["size"]) * scale)
                page.insert_text(point, value, fontsize=size, fontname=face,
                                 render_mode=3, morph=(point, pdf.Matrix(stretch, 1)))
                x += width * stretch

    for content in learning["backPages"]:
        learning_page(content)
    page = heading_page("关键词索引")
    paragraph(page, 44, 184, "按需要点击关键词，直接阅读相关教程或图解。中文与英文名称指向同一份说明。", height=80)
    for index, item in enumerate(learning["index"]):
        column, row = divmod(index, 12)
        if column > 1:
            raise ValueError("关键词超过两栏，需要扩充索引")
        x, y = 44 + column * 322, 296 + row * 45
        text(page, x, y, item["term"], 12, blue)
        link(page, pdf.Rect(x, y - 18, x + 310, y + 12), item["target"])
    for source_page, rect, page_number in links:
        doc[source_page].insert_link({"kind": pdf.LINK_GOTO, "from": rect, "page": page_number})
    doc.set_toc(outlines)
    doc.set_metadata({"title": "Harken Search 新手入门与功能手册",
                      "author": "Harken Search", "subject": "离线教程与可检索图解"})
    doc.subset_fonts()
    content = doc.tobytes(garbage=4, deflate=True, no_new_id=True)
    page_count = len(doc)
    doc.close()
    receipt = {"schemaVersion": 3, "sourceSignature": signature,
               "pdfSha256": hashlib.sha256(content).hexdigest(),
               "imageCount": len(pages), "introductionPageCount": 2,
               "learningPageCount": len(learning["frontPages"]) + len(learning["backPages"]),
               "searchableImageCount": len(pages), "pageCount": page_count}
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".pdf.tmp")
    temporary.write_bytes(content)
    temporary.replace(output)
    output.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_PDF)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--stage-dir", type=Path)
    args = parser.parse_args()
    try:
        receipt = check_pdf(args.source_dir, args.output) if args.check or args.stage_dir else generate(args.source_dir, args.output)
        if args.stage_dir:
            stage_manual(args.stage_dir, source=args.source_dir, output=args.output)
        print(json.dumps({"pdf": str(args.output), **receipt}, ensure_ascii=False))
        return 0
    except (OSError, ValueError, KeyError, ImportError) as error:
        print(f"离线手册未生成或未打包：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
