"""raw_canvas.py - 裸机V2渲染核心（复刻 gfx_raw.c + font_raw.h），供 render_raw_main.py 调用"""
import os, re
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
W = H = 240

def hx(c): return ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)

CLR_BG = 0x0B0E13; CLR_TEXT = 0xFFFFFF; CLR_MUTED = 0x8A8F96; CLR_LINE = 0x22262D
CLR_SHELL = 0x9AA0A8; CLR_RANGE = 0x5DC8E8; CLR_DISCH = 0xF0997B; CLR_CHG = 0x67C98A
CLR_LOW = 0xF05555; CLR_TAG_BG_D = 0x3A2410; CLR_TAG_BG_C = 0x12301C; CLR_CAP_TXT = 0xB8BDC4
BX, BY, BW, BH, BIW = 16, 28, 196, 96, 188

def load_font_raw(path):
    txt = open(path, encoding="utf-8").read()
    m = re.search(r"FONT_RAW_BLOB\[\] = \{(.*?)\};", txt, re.S)
    blob = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
    fonts = {}
    for name in ["CN12", "NUM16", "NUM40"]:
        mm = re.search(r"FONT_%s\[\] = \{(.*?)\};" % name, txt, re.S)
        g = {}
        pats = re.findall(r"\{0x([0-9A-Fa-f]+),\s*(\d+),\s*(\d+),\s*(\d+|0xFFFFFFFFu),\s*(\d+)\}", mm.group(1))
        for cp, w, h, off, ln in pats:
            g[int(cp, 16)] = (int(w), int(h), None if "FF" in off else int(off), int(ln))
        fonts[name.lower()] = g
    return blob, fonts

BLOB, FONTS = load_font_raw(os.path.join(OUT, "font_raw.h"))

def soc_rgb(soc):
    import sys
    sys.path.insert(0, OUT)
    from render_verify import hsv_to_rgb_lvgl
    return hsv_to_rgb_lvgl(soc * 12 // 10, 78, 86)

class CV:
    def __init__(self):
        self.img = Image.new("RGB", (W, H), hx(CLR_BG))
        self.px = self.img.load()
    def fill(self, x, y, w, h, c):
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(W, x + w), min(H, y + h)
        if x1 <= x0 or y1 <= y0: return
        rgb = hx(c)
        for yy in range(y0, y1):
            for xx in range(x0, x1):
                self.px[xx, yy] = rgb
    def _keep(self, dx, dy, r):
        # 角部第dy行(0=top)应保留的宽度：圆心在内角(r,r)，找最左满足
        # (x-r)^2+(dy-r)^2<=r^2 的x，keep=r-x。dx参数忽略(兼容旧调用)。
        for x in range(r):
            xx = x - r
            yy = dy - r
            if xx * xx + yy * yy <= r * r:
                return r - x
        return 1
