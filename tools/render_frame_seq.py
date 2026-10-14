#!/usr/bin/env python3
"""render_frame_seq.py - 按 C 代码 ui_raw_draw_static/ui_raw_update 的分层
序列逐帧模拟渲染，排查"部分元素不上屏"类问题。

用法: python3 render_frame_seq.py
输出: images/sim_step1_static.png      静态层后（壳/标签/前后缀/静态数值）
      images/sim_step2_frame0.png      update 首帧后（胶囊/SOC/续航/填充条）
      images/sim_step3_soc13.png       soc=13% 若干帧后
      images/sim_step4_float_fail.png  模拟 newlib-nano float printf 失效
                                       （snprintf "%.2f" 输出空串）的首帧
"""
import os, sys
OUT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, OUT)
IMG = os.path.join(OUT, "..", "images")
from raw_draw import _rrect, _rborder, _gw, _text
from raw_canvas import CV, CLR_BG, CLR_TEXT, CLR_MUTED, CLR_LINE, CLR_SHELL
from raw_canvas import CLR_RANGE, CLR_DISCH, CLR_CHG, CLR_LOW
from raw_canvas import CLR_TAG_BG_D, CLR_TAG_BG_C, CLR_CAP_TXT, soc_rgb
from raw_canvas import BX, BY, BW, BH, BIW

CAP_PRE, CAP_SUF, CAPV_W = "自学习容量：", "(±5%)", 62


def fmt_fx(v, dec):
    """复刻 C fmt_fx：整数运算格式化（float printf 正常时的预期输出）"""
    scale = 10 ** dec
    n = int(abs(v) * scale + 0.5)
    return "%s%d.%0*d" % ("-" if v < 0 else "", n // scale, dec, n % scale)


def draw_static(cv, volt, curr, pkw, cap, charging, float_ok=True):
    """复刻 ui_raw_draw_static(&d)"""
    cv.fill(0, 0, 240, 240, CLR_BG)
    _rborder(cv, BX, BY, BW, BH, 18, 3, CLR_SHELL)
    _rrect(cv, BX + BW + 3, 52, 9, 48, 4, CLR_SHELL)
    cv.fill(24, 156, 192, 1, CLR_LINE)
    cv.fill(24, 204, 192, 1, CLR_LINE)
    for cx, unit in ((60, "电压 V"), (120, "电流 A"), (180, "功率 kW")):
        _text(cv, cx - 36 + (72 - _gw("cn12", unit)) // 2, 184,
              "cn12", unit, CLR_MUTED)
    w1, w3 = _gw("cn12", CAP_PRE), _gw("cn12", CAP_SUF)
    x = (240 - (w1 + 2 + CAPV_W + 2 + w3)) // 2
    cap_vx = x + w1 + 2
    _text(cv, x, 210, "cn12", CAP_PRE, CLR_CAP_TXT)
    _text(cv, x + w1 + 2 + CAPV_W + 2, 210, "cn12", CAP_SUF, CLR_CAP_TXT)
    # 静态数值（C: snprintf "%.2f" 等 —— float printf 失效时输出空串）
    a = abs(curr)
    v0 = fmt_fx(volt, 2) if float_ok else ""
    v1 = fmt_fx(a, 1) if float_ok else ""
    v2 = fmt_fx(pkw, 2) if float_ok else ""
    cv_ = (fmt_fx(cap, 1) + "Ah") if float_ok else ""
    for cx, val, col in ((60, v0, CLR_TEXT),
                         (120, ("+" if charging else "-") + v1,
                          CLR_CHG if charging else CLR_DISCH),
                         (180, v2, CLR_TEXT)):
        _text(cv, cx - 36 + (72 - _gw("num16", val)) // 2, 162, "num16", val, col)
    _text(cv, cap_vx + (CAPV_W - _gw("num16", cv_)) // 2, 209, "num16", cv_, CLR_TEXT)


def draw_update(cv, soc, rng, charging):
    """复刻 ui_raw_update 首帧（无快照/无并集的首帧路径）"""
    soc = max(0, min(100, int(soc + 0.5)))
    scol = soc_rgb(soc)
    scol565px = (scol[0], scol[1], scol[2])
    _rrect(cv, 92, 6, 56, 20, 10, CLR_BG)          # 胶囊擦
    tag = "充电中" if charging else "放电中"
    _rrect(cv, 92, 6, 56, 20, 10, CLR_TAG_BG_C if charging else CLR_TAG_BG_D)
    _text(cv, 92 + (56 - _gw("cn12", tag)) // 2, 9, "cn12", tag,
          CLR_CHG if charging else CLR_DISCH)
    fw = (BIW * soc) // 100
    if fw > 0:
        _rrect(cv, BX + 4, BY + 4, fw, BH - 8, 13,
               (scol565px[0] << 16) | (scol565px[1] << 8) | scol565px[2])
    s = "%d%%" % soc
    # SOC 槽底色分段复刻：fill_end = BX+4+fw，盒内左段填充色右段 bg
    x0, x1 = BX + (BW - _gw("num40", s)) // 2, 0
    tw = _gw("num40", s); x1 = x0 + tw
    fill_end = BX + 4 + fw
    seg = max(x0, min(fill_end, x1))
    if seg > x0:
        cv.fill(x0, BY + 25, seg - x0, 48,
                (scol565px[0] << 16) | (scol565px[1] << 8) | scol565px[2])
    if x1 > seg:
        cv.fill(seg, BY + 25, x1 - seg, 48, CLR_BG)
    _text(cv, x0, BY + 25, "num40", s, CLR_LOW if soc <= 10 else CLR_TEXT)
    rt = ("续航 %d km" % rng) if rng >= 0 else "续航 --"
    _text(cv, (240 - _gw("cn12", rt)) // 2, 130, "cn12", rt, CLR_RANGE)


if __name__ == "__main__":
    d = dict(volt=72.5, curr=-12.3, pkw=0.89, cap=19.6, charging=0)
    # step1: 静态层
    cv = CV(); draw_static(cv, **d)
    cv.img.save(os.path.join(IMG, "sim_step1_static.png"))
    # step2: + update 首帧（soc 从 0 起 +0.5 -> 1%）
    draw_update(cv, soc=0.5, rng=0, charging=0)
    cv.img.save(os.path.join(IMG, "sim_step2_frame0.png"))
    # step3: soc 走到 13%
    cv2 = CV(); draw_static(cv2, **d)
    draw_update(cv2, soc=12.5, rng=9, charging=0)
    cv2.img.save(os.path.join(IMG, "sim_step3_soc13.png"))
    # step4: float printf 失效场景（%.2f/%.1f 输出空串）
    cv3 = CV(); draw_static(cv3, **d, float_ok=False)
    draw_update(cv3, soc=0.5, rng=0, charging=0)
    cv3.img.save(os.path.join(IMG, "sim_step4_float_fail.png"))
    print("生成 sim_step1_static / sim_step2_frame0 / sim_step3_soc13 / sim_step4_float_fail")
