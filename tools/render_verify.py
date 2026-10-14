"""
render_verify.py - 复刻 ui_lvgl.c 的绘图逻辑，逐像素渲染成
LVGL 图片源文件（lv_img_dsc_t），供 lvgl_image_viewer.py 转 PNG 做视觉验证。

同时输出布局体检报告：元素越界、文字超框、flex 行溢出等问题。
"""
import os
from PIL import Image, ImageDraw, ImageFont

W = H = 240
OUT = os.path.dirname(os.path.abspath(__file__))

F_CN12 = "C:/Windows/Fonts/simhei.ttf"
F_CN12B = "C:/Windows/Fonts/simhei.ttf"
F_NUM40 = "C:/Windows/Fonts/arialbd.ttf"   # 近似 Montserrat 40
F_NUM16 = "C:/Windows/Fonts/arial.ttf"     # 近似 Montserrat 16

# ---- LVGL 颜色常量（与 ui_lvgl.c 一致）----
CLR_BG = 0x0B0E13
CLR_TEXT = 0xFFFFFF
CLR_MUTED = 0x8A8F96
CLR_LINE = 0x22262D
CLR_SHELL = 0x9AA0A8
CLR_RANGE = 0x5DC8E8
CLR_DISCH = 0xF0997B
CLR_CHG = 0x67C98A
CLR_LOW = 0xF05555
CLR_TAG_BG_D = 0x3A2410
CLR_TAG_BG_C = 0x12301C
CLR_CAP_TXT = 0xB8BDC4

BATT_X, BATT_Y, BATT_W, BATT_H = 16, 28, 196, 96
BATT_INNER_W = 188

REPORT = []


def hx(c):
    return ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)


def font(path, size):
    return ImageFont.truetype(path, size)


def tw(draw, txt, f):
    """文本像素宽度"""
    return int(draw.textlength(txt, font=f))


class Canvas:
    def __init__(self):
        self.img = Image.new("RGB", (W, H), hx(CLR_BG))
        self.d = ImageDraw.Draw(self.img)

    def rr(self, x, y, w, h, r, fill=None, outline=None, width=1):
        if w <= 0 or h <= 0:
            return
        box = [x, y, x + w - 1, y + h - 1]
        if box[2] < box[0] or box[3] < box[1]:
            return
        if r > 0:
            self.d.rounded_rectangle(box, radius=r, fill=hx(fill) if fill is not None else None,
                                     outline=hx(outline) if outline else None, width=width)
        else:
            self.d.rectangle(box, fill=hx(fill) if fill is not None else None,
                             outline=hx(outline) if outline else None, width=width)

    def label(self, x, y, w, h, txt, f, color, line_h, clip=True):
        """复刻 mk_label：文字水平居中，垂直按 (h - line_h)/2 上边距，CLIP 模式"""
        t = tw(self.d, txt, f)
        # LVGL LV_LABEL_LONG_CLIP：超出宽度则被裁掉
        if clip and t > w:
            REPORT.append(f"[裁剪] '{txt}' 文本宽 {t}px > 框宽 {w}px（框 x={x}），会丢字")
        tx = x + (w - t) // 2
        ty = y + max(0, (h - line_h)) // 2
        # CLIP：限制在框内
        if clip and t > w:
            old = self.img.copy()
            self.img.paste(old, (0, 0))
            layer = Image.new("RGB", (W, H), hx(CLR_BG))
            ld = ImageDraw.Draw(layer)
            ld.text((tx, ty), txt, font=f, fill=hx(color))
            mask = Image.new("L", (W, H), 0)
            ImageDraw.Draw(mask).rectangle([x, y, x + w, y + h], fill=255)
            self.img = Image.composite(layer, self.img, mask)
            self.d = ImageDraw.Draw(self.img)
        else:
            self.d.text((tx, ty), txt, font=f, fill=hx(color))

    def check_bounds(self, name, x, y, w, h):
        if x < 0 or y < 0 or x + w > W or y + h > H:
            REPORT.append(f"[越界] {name} 区域 x{x}~{x+w} y{y}~{y+h} 超出 240x240")


def hsv_to_rgb_lvgl(h, s, v):
    """复刻 LVGL v8 官方 lv_color_hsv_to_rgb：h 0..359, s/v 0..100（百分比）"""
    h = (h * 255) // 360
    s = (s * 255) // 100
    v = (v * 255) // 100
    if s == 0:
        return (v, v, v)
    region = h // 43
    remainder = (h - region * 43) * 6
    p = (v * (255 - s)) >> 8
    q = (v * (255 - ((s * remainder) >> 8))) >> 8
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8
    tbl = {0: (v, t, p), 1: (q, v, p), 2: (p, v, t), 3: (p, q, v), 4: (t, p, v)}
    return tbl.get(region, (v, p, q))


# ============================ 复刻 ui_lvgl.c ============================
def render_lvgl(soc=76, charging=0, volt=84.52, curr=-12.3, pkw=1.04,
                cap=19.6, rng=50):
    cv = Canvas()
    cn12 = font(F_CN12, 12)
    num40 = font(F_NUM40, 40)
    num16 = font(F_NUM16, 16)

    # 状态胶囊 92,6,56x20
    cv.rr(92, 6, 56, 20, 10, fill=CLR_TAG_BG_C if charging else CLR_TAG_BG_D)
    tag = "充电中" if charging else "放电中"
    cv.label(92, 6, 56, 20, tag, cn12, CLR_CHG if charging else CLR_DISCH, 12)

    # 电池外壳：透明底 + 3px 灰描边
    cv.rr(BATT_X, BATT_Y, BATT_W, BATT_H, 18, outline=CLR_SHELL, width=3)
    # 电极头
    cv.rr(BATT_X + BATT_W + 3, 52, 9, 48, 4, fill=CLR_SHELL)
    cv.check_bounds("电极头", BATT_X + BATT_W + 3, 52, 9, 48)

    # 填充条
    fw = (BATT_INNER_W * soc) // 100
    if fw > 0:
        col = hsv_to_rgb_lvgl(soc * 12 // 10, 78, 86)
        cv.rr(BATT_X + 4, BATT_Y + 4, fw, BATT_H - 8, 13, fill=(col[0] << 16) | (col[1] << 8) | col[2])

    # SOC 大字
    soc_col = CLR_LOW if soc <= 10 else CLR_TEXT
    cv.label(BATT_X, BATT_Y, BATT_W, BATT_H, f"{soc}%", num40, soc_col, 46)

    # 续航
    rt = f"续航 {rng} km" if rng >= 0 else "续航 --"
    cv.label(16, 130, 208, 16, rt, cn12, CLR_RANGE, 12)

    # 分隔线
    cv.rr(24, 156, 192, 1, 0, fill=CLR_LINE)
    cv.rr(24, 204, 192, 1, 0, fill=CLR_LINE)

    # 三列
    for cx, val, unit, col in (
        (60, f"{volt:.2f}", "电压 V", CLR_TEXT),
        (120, f"{'+' if charging else '-'}{abs(curr):.1f}", "电流 A", CLR_CHG if charging else CLR_DISCH),
        (180, f"{pkw:.2f}", "功率 kW", CLR_TEXT),
    ):
        cv.label(cx - 36, 162, 72, 18, val, num16, col, 18)
        cv.label(cx - 36, 182, 72, 14, unit, cn12, CLR_MUTED, 12)

    # 底部容量行：LVGL flex row 居中排布
    pre, val2, suf = "自学习容量：", (f"{cap:.1f}Ah" if cap >= 0 else "--"), "(±5%)"
    w1, w2, w3 = tw(cv.d, pre, cn12), tw(cv.d, val2, num16), tw(cv.d, suf, cn12)
    total = w1 + w2 + w3
    if total > W:
        REPORT.append(f"[溢出] 底部容量行总宽 {total}px > 屏宽 240px，左右各被裁 {(total-240)//2}px")
    x = (W - total) // 2
    cv.label(x, 208, w1, 18, pre, cn12, CLR_CAP_TXT, 12, clip=False)
    cv.label(x + w1, 208, w2, 18, val2, num16, CLR_TEXT, 18, clip=False)
    cv.label(x + w1 + w2, 208, w3, 18, suf, cn12, CLR_CAP_TXT, 12, clip=False)

    return cv.img


# ============================ 输出 LVGL 图片源 ============================
def to_rgb565(img):
    px = img.load()
    data = []
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            data.append(v)
    return data


def write_lvgl_c(img, path, name):
    data = to_rgb565(img)
    # uint8_t[] 里 RGB565 必须按小端拆成两个字节：0xF8,0x00
    buf = bytearray()
    for v in data:
        buf.append(v & 0xFF)
        buf.append((v >> 8) & 0xFF)
    lines = []
    for i in range(0, len(buf), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in buf[i:i + 16]) + ",")
    body = "\n".join(lines)
    src = f"""/**
 * {name}.c - 由 render_verify.py 生成的界面位图（RGB565, 240x240）
 *
 * 这是 ui_lvgl.c 布局的像素级预览图，可直接嵌进 LVGL 做静态背景或离线校对：
 *   extern const lv_img_dsc_t {name};
 *   lv_img_set_src(img, &{name});
 */
#include "lvgl.h"

const lv_img_dsc_t {name} = {{
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.w = 240,
    .header.h = 240,
    .header.stride = 480,
    .data_size = 240 * 240 * 2,
    .data = (const uint8_t[]) {{
{body}
    }},
}};
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write(src)
    return path


if __name__ == "__main__":
    a = render_lvgl()
    b = render_lvgl(soc=18, charging=1, volt=66.30, curr=8.5, pkw=0.56, cap=-1, rng=-1)
    d = render_lvgl(soc=0)

    a.save(os.path.join(OUT, "preview_lvgl_76.png"))
    b.save(os.path.join(OUT, "preview_lvgl_charging.png"))
    d.save(os.path.join(OUT, "preview_lvgl_0.png"))

    p1 = write_lvgl_c(a, os.path.join(OUT, "ui_lvgl_screen.c"), "ui_scr_76")
    p2 = write_lvgl_c(b, os.path.join(OUT, "ui_lvgl_screen_chg.c"), "ui_scr_charging")

    print("生成完成：")
    for p in (p1, p2):
        print("  ", os.path.basename(p), os.path.getsize(p), "bytes")
    print("\n=== 布局体检 ===")
    if REPORT:
        for r in sorted(set(REPORT)):
            print("  !", r)
    else:
        print("   无越界/裁剪问题")
