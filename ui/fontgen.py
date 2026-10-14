#!/usr/bin/env python3
"""fontgen.py - 从系统字体提取 1-bit 点阵子集，生成 font_raw.h

用法: python3 ui/fontgen.py   # 生成同目录下的 ui/font_raw.h
字体: NotoSansCJK(中文12px) + DejaVuSans(数字16/40px)
字符: 与 ui_lvgl.c / README 完全一致的子集
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
CN_FONT = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
EN_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
EN_FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# ---- 子集：界面实际用到的全部字符 ----
# LVGL版文案: 放电中/充电中/续航 xx km/电压 V/电流 A/功率 kW/自学习容量：xx.xAh(±5%)
CN12_CHARS = "放电压流功率续航自学习容量差误充中：±%()kkmMAhVW.0123456789-+ "
NUM16_CHARS = "0123456789.+-AhkkmMVW% "
NUM40_CHARS = "0123456789%"

FONTS = [
    ("cn12", CN_FONT, 12, CN12_CHARS, False),
    ("num16", EN_FONT, 16, NUM16_CHARS, False),
    ("num40", EN_FONT_B, 40, NUM40_CHARS, True),
]


def render_glyph(font_path, size, ch, bold=False):
    """整cell渲染：宽=advance，高=ascent+descent，基线天然对齐。
    不做紧裁剪，. k W ( ± 等小字与数字同基线；左侧按advance排，保证字间距。
    代价：每字形多几字节空白，74字形总共多 ~1KB，可接受。"""
    f = ImageFont.truetype(font_path, size)
    asc, desc = f.getmetrics()
    cell_h = asc + desc
    tmp = ImageDraw.Draw(Image.new("L", (8, 8)))
    adv = int(tmp.textlength(ch, font=f))
    if adv <= 0:
        adv = size // 2
    W = adv
    img = Image.new("L", (W, cell_h), 0)
    d = ImageDraw.Draw(img)
    # anchor="ls": 左端基线对齐，基线 y=asc
    d.text((0, asc), ch, font=f, fill=255, anchor="ls")
    px = img.load()
    flat = []
    for y in range(cell_h):
        bits = 0
        for x in range(W):
            bits = (bits << 1) | (1 if px[x, y] > 100 else 0)
            if (x + 1) % 8 == 0:
                flat.append(bits)
                bits = 0
        if W % 8:
            bits <<= (8 - W % 8)
            flat.append(bits)
    return W, cell_h, flat


def main():
    glyphs = {}  # (font, ch) -> (w,h,offset,size)
    blob = bytearray()
    for fname, fpath, size, chars, bold in FONTS:
        for ch in dict.fromkeys(chars):
            if ch == " ":
                # 空格：宽度取数字一半，不存位图
                f = ImageFont.truetype(fpath, size)
                w = int(ImageDraw.Draw(Image.new("L", (8, 8))).textlength("0", font=f) // 2)
                glyphs[(fname, ch)] = (w, size, -1, 0)
                continue
            w, h, flat = render_glyph(fpath, size, ch, bold)
            off = len(blob)
            blob.extend(flat)
            glyphs[(fname, ch)] = (w, h, off, len(flat))
    # 写出C头
    lines = []
    lines.append("/**")
    lines.append(" * font_raw.h - 由 fontgen.py 自动生成的 1-bit 点阵子集（勿手改）")
    lines.append(" *   cn12 : NotoSansCJK 12px 中文+符号")
    lines.append(" *   num16: DejaVuSans 16px 数字+单位")
    lines.append(" *   num40: DejaVuSans-Bold 40px 大字SOC")
    lines.append(" * 总位图 %d bytes" % len(blob))
    lines.append(" */")
    lines.append("#ifndef FONT_RAW_H")
    lines.append("#define FONT_RAW_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("static const uint8_t FONT_RAW_BLOB[] = {")
    for i in range(0, len(blob), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in blob[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("typedef struct { uint32_t cp; uint8_t w, h; uint32_t off; uint16_t len; } raw_glyph_t;")
    lines.append("")
    for fname, fpath, size, chars, bold in FONTS:
        lines.append("/* %s %dpx */" % (fname, size))
        lines.append("static const raw_glyph_t FONT_%s[] = {" % fname.upper())
        for ch in dict.fromkeys(chars):
            w, h, off, ln = glyphs[(fname, ch)]
            cp = ord(ch)
            if ch == " ":
                lines.append("    {0x20, %d, %d, 0xFFFFFFFFu, 0},  /* space */" % (w, h))
            else:
                tag = ch if ord(ch) < 128 else ("U+%04X" % cp)
                lines.append("    {0x%04X, %d, %d, %d, %d},  /* %s */" % (cp, w, h, off, ln, tag))
        lines.append("};")
        lines.append("#define FONT_%s_N (sizeof(FONT_%s)/sizeof(FONT_%s[0]))" % (fname.upper(), fname.upper(), fname.upper()))
        lines.append("")
    lines.append("#endif /* FONT_RAW_H */")
    path = os.path.join(OUT_DIR, "font_raw.h")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("生成 %s: blob=%d bytes glyphs=%d" % (path, len(blob), len(glyphs)))


if __name__ == "__main__":
    main()
