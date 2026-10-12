"""接 raw_canvas.CV：圆角/文字/整屏渲染。由 render_raw_main.py 调用。"""
from raw_canvas import CV, BLOB, FONTS, soc_rgb, hx
from raw_canvas import CLR_BG, CLR_TEXT, CLR_MUTED, CLR_LINE, CLR_SHELL
from raw_canvas import CLR_RANGE, CLR_DISCH, CLR_CHG, CLR_LOW
from raw_canvas import CLR_TAG_BG_D, CLR_TAG_BG_C, CLR_CAP_TXT
from raw_canvas import BX, BY, BW, BH, BIW, W, H

def _rrect(cv, x, y, w, h, r, c):
    if r <= 0:
        cv.fill(x, y, w, h, c); return
    r = min(r, w // 2, h // 2)
    cv.fill(x, y + r, w, h - 2 * r, c)
    for yy in range(r):
        keep = cv._keep(r - 1, yy, r)
        xl = x + (r - keep); ww = w - 2 * (r - keep)
        cv.fill(xl, y + yy, ww, 1, c)
        cv.fill(xl, y + h - 1 - yy, ww, 1, c)
    cv.fill(x, y + r, r, h - 2 * r, c)
    cv.fill(x + w - r, y + r, r, h - 2 * r, c)

def _rborder(cv, x, y, w, h, r, lw, c):
    # 与 gfx_raw.c 同算法：外框实心圆角 - 内框背景挖空，保证四角连续
    _rrect(cv, x, y, w, h, r, c)
    _rrect(cv, x + lw, y + lw, w - 2 * lw, h - 2 * lw,
           r - lw if r > lw else 0, CLR_BG)

def _gw(tab, s):
    w = 0
    for ch in s:
        g = FONTS[tab].get(ord(ch))
        if g: w += g[0] + 1
    return max(0, w - 1)

def _text(cv, x, y, tab, s, c):
    cx = x; rgb = hx(c)
    for ch in s:
        g = FONTS[tab].get(ord(ch))
        if not g or g[2] is None:
            cx += (g[0] if g else 6) + 1; continue
        gw, gh, off, ln = g
        st = (gw + 7) // 8
        for row in range(gh):
            for col in range(gw):
                if BLOB[off + row * st + col // 8] & (0x80 >> (col % 8)):
                    if 0 <= cx+col < W and 0 <= y+row < H:
                        cv.px[cx+col, y+row] = rgb
        cx += gw + 1

def render_raw(soc=76, charging=0, volt=84.52, curr=-12.3, pkw=1.04, cap=19.6, rng=50):
    cv = CV()
    soc = max(0, min(100, int(soc + 0.5)))
    scol = (soc_rgb(soc)[0] << 16) | (soc_rgb(soc)[1] << 8) | soc_rgb(soc)[2]
    cv.fill(0, 0, 240, 240, CLR_BG)
    if charging:
        _rrect(cv, 92, 6, 56, 20, 10, CLR_TAG_BG_C)
        tw = _gw("cn12", "充电中")
        _text(cv, 92 + (56 - tw) // 2, 9, "cn12", "充电中", CLR_CHG)
    else:
        _rrect(cv, 92, 6, 56, 20, 10, CLR_TAG_BG_D)
        tw = _gw("cn12", "放电中")
        _text(cv, 92 + (56 - tw) // 2, 9, "cn12", "放电中", CLR_DISCH)
    _rborder(cv, BX, BY, BW, BH, 18, 3, CLR_SHELL)
    _rrect(cv, BX + BW + 3, 52, 9, 48, 4, CLR_SHELL)
    fw = (BIW * soc) // 100
    if fw > 0: _rrect(cv, BX + 4, BY + 4, fw, BH - 8, 13, scol)
    s = "%d%%" % soc
    _text(cv, BX + (BW - _gw("num40", s)) // 2, BY + 25,
          "num40", s, CLR_LOW if soc <= 10 else CLR_TEXT)
    rt = ("续航 %d km" % rng) if rng >= 0 else "续航 --"
    _text(cv, (240 - _gw("cn12", rt)) // 2, 130, "cn12", rt, CLR_RANGE)
    cv.fill(24, 156, 192, 1, CLR_LINE)
    cv.fill(24, 204, 192, 1, CLR_LINE)
    rows = (
        (60, "%.2f" % volt, "电压 V", CLR_TEXT),
        (120, "%s%.1f" % ("+" if charging else "-", abs(curr)), "电流 A",
         CLR_CHG if charging else CLR_DISCH),
        (180, "%.2f" % pkw, "功率 kW", CLR_TEXT),
    )
    for cx, val, unit, col in rows:
        _text(cv, cx - 36 + (72 - _gw("num16", val)) // 2, 162, "num16", val, col)
        _text(cv, cx - 36 + (72 - _gw("cn12", unit)) // 2, 184, "cn12", unit, CLR_MUTED)
    pre, suf = "自学习容量：", "(±5%)"
    capv = ("%.1fAh" % cap) if cap >= 0 else "--"
    w1, w2, w3 = _gw("cn12", pre), _gw("num16", capv), _gw("cn12", suf)
    x = (240 - (w1 + 2 + w2 + 2 + w3)) // 2
    _text(cv, x, 210, "cn12", pre, CLR_CAP_TXT)
    _text(cv, x + w1 + 2, 209, "num16", capv, CLR_TEXT)
    _text(cv, x + w1 + 2 + w2 + 2, 210, "cn12", suf, CLR_CAP_TXT)
    return cv.img
