#!/usr/bin/env python3
"""render_raw_main.py - 输出裸机V2预览 + LVGL并排对比。用法: python3 render_raw_main.py"""
import os
from PIL import Image
from raw_draw import render_raw

OUT = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(OUT, "..", "images")
W = H = 240

if __name__ == "__main__":
    import sys
    sys.path.insert(0, OUT)
    import render_verify as _lv
    for _a, _n in [
        ("F_CN12", "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
        ("F_CN12B", "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
        ("F_NUM40", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        ("F_NUM16", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
    ]:
        try: setattr(_lv, _a, _n)
        except Exception: pass
    render_lvgl = _lv.render_lvgl
    has = True
    cases = [
        ("76", dict(soc=76, charging=0, volt=84.52, curr=-12.3, pkw=1.04, cap=19.6, rng=50)),
        ("charging", dict(soc=18, charging=1, volt=66.30, curr=8.5, pkw=0.56, cap=-1, rng=-1)),
        ("0", dict(soc=0, charging=0, volt=48.02, curr=-0.5, pkw=0.02, cap=0.0, rng=0)),
    ]
    for tag, kw in cases:
        raw = render_raw(**kw)
        raw.save(os.path.join(IMG, "preview_raw_%s.png" % tag))
        print("生成 preview_raw_%s.png" % tag)
        lvim = render_lvgl(**kw).convert("RGB")
        combo = Image.new("RGB", (W * 2 + 8, H), (40, 40, 40))
        combo.paste(lvim, (0, 0)); combo.paste(raw, (W + 8, 0))
        combo.save(os.path.join(IMG, "compare_%s.png" % tag))
        a, b = lvim.load(), raw.load()
        d = sum(1 for y in range(H) for x in range(W) if a[x, y] != b[x, y])
        print("  compare_%s diff %d/57600 (%.1f%%)" % (tag, d, d / 576.0))
    print("完成。左=LVGL 右=裸机V2，差异主要来自字体抗锯齿。")
