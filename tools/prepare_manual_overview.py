"""将指定飞书整屏截图中的私有素材换成示例；仅处理这一张已核对的原图。"""
import argparse
import hashlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

SOURCE_SHA256 = "8cfd16fd4e59542f5c773cc52a49faf8e15a102dc06867881786e00bfa6e6694"
ROOT = Path(__file__).resolve().parents[1]


def prepare(source: Path, output: Path, font_path: Path):
    if source.resolve() == output.resolve():
        raise ValueError("必须另存副本，不能覆盖原图")
    if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError("原图已改变，须重新核对脱敏位置")
    with Image.open(source) as raw:
        image = raw.convert("RGB")
    if image.size != (2557, 1368):
        raise ValueError("原图尺寸已改变")
    draw = ImageDraw.Draw(image)
    sx, sy = image.width / 2048, image.height / 1096
    font = ImageFont.truetype(str(font_path), 13)

    def fill(box, color="#17202A"):
        draw.rectangle(tuple(round(v * (sx if i % 2 == 0 else sy)) for i, v in enumerate(box)), fill=color)

    def label(x, y, value, color="#C9D2DA"):
        draw.text((round(x * sx), round(y * sy)), value, font=font, fill=color)

    # 路径、库名、文件名和数量采用不透明覆盖；波形、图标、边框及按钮保留。
    fill((382, 56, 463, 69), "#253545")
    label(383, 56, "全部文件 (32)")
    fill((390, 84, 1480, 108), "#111820")
    label(393, 91, "路径:  Demo Library  >  Sound Effects  >  Collection")
    fill((68, 197, 350, 274), "#0E151D")
    for row, value in enumerate(["Demo Library [只读库]", "Local Audio", "Sound Effects", "Project Sounds", "Collection"]):
        label(69, 201 + row * 13.7, value)
    fill((454, 129, 665, 713))
    for row in range(23):
        label(458, 130 + row * 25.6, f"Demo_Sound_{row + 1:03}.wav")

    # 用普通色块保留封面网格的结构，不保留第三方封面像素。
    fill((1508, 106, 2035, 359), "#10171E")
    colors = ["#344C5D", "#526765", "#565565", "#5F665A", "#536578"]
    for row in range(4):
        for column in range(8):
            x, y = 1513 + column * 65.5, 110 + row * 65.5
            fill((x, y, x + 60, min(y + 59, 357)), colors[(row + column) % len(colors)])
            label(x + 9, y + 24, f"ART {row * 8 + column + 1:02}")
    fill((1545, 427, 1850, 448), "#151D25")
    label(1547, 431, "Demo_Sound_001.wav")
    fill((1502, 471, 1730, 488), "#151D25")
    label(1506, 473, "已索引: 32 / 32 个文件")
    fill((1670, 500, 1875, 516), "#212A36")
    label(1680, 501, "开始批量特征分析")
    fill((1508, 584, 1706, 713), "#17202A")
    for row in range(5):
        label(1512, 587 + row * 29, f"Similar_Sound_{row + 1:03}.wav")
    for x, right, name in [(30, 108, "Project A"), (138, 219, "Project B"), (243, 323, "Foley"), (359, 435, "Ambience"), (471, 541, "Impacts"), (576, 650, "Whooshes"), (685, 751, "Favorites")]:
        fill((x, 746, right, 762), "#1B232D")
        label(x, 749, name)
    fill((1572, 784, 1638, 801), "#17202A")
    label(1584, 787, "32 个文件")
    fill((1647, 784, 1747, 800), "#D5E0F0")
    label(1650, 786, "Demo Library", "#203040")
    fill((92, 1070, 900, 1086), "#10171E")
    label(98, 1072, "Demo_Sound_001.wav")
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, format="PNG")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/manual/ui-overview.png")
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    prepare(args.source, args.output, args.font)
