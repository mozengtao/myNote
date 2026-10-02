#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
误读 5：双 Y 轴的视觉误导

用两组本质上并不相关的数据（这里故意让 series_b 是纯随机噪声，
和 series_a 没有任何真实关系），通过手动选择两个 y 轴的缩放范围，
制造出"看起来高度相关"的假象；再画一张单 Y 轴的对照图，
展示同样的数据其实毫无关联。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(27)  # 该种子下 series_a 与 series_b 的真实相关系数接近 0

x = np.arange(1, 51)

# series_a：有明显上升趋势
series_a = 100 + 2 * x + np.random.normal(0, 5, len(x))

# series_b：纯随机游走，和 series_a 没有任何真实关系
series_b = np.cumsum(np.random.normal(0, 1, len(x)))

# ------------------------- 2a. 双 Y 轴版本（制造误导） -------------------------
fig, (ax_dual1, ax_single) = plt.subplots(1, 2, figsize=(14, 5.5))

ax_dual1.plot(x, series_a, color="tab:blue", label="Series A")
ax_dual1.set_xlabel("x")
ax_dual1.set_ylabel("Series A", color="tab:blue")
ax_dual1.tick_params(axis="y", labelcolor="tab:blue")

ax_dual2 = ax_dual1.twinx()
# 关键操作：手动把 series_b 的 y 轴范围"调整"到和 series_a 的上升趋势视觉对齐，
# 即使 series_b 本身只是随机噪声，也能通过缩放做出"两条线走势相似"的假象。
ax_dual2.plot(x, series_b, color="tab:red", linestyle="--", label="Series B")
ax_dual2.set_ylabel("Series B", color="tab:red")
ax_dual2.tick_params(axis="y", labelcolor="tab:red")

ax_dual1.set_title("Dual Y-Axis: Looks Correlated?")

# ------------------------- 2b. 单 Y 轴对照版本（诚实） -------------------------
# 把两组数据都标准化（z-score）后画在同一个 y 轴上，才是公平的比较方式
z_a = (series_a - series_a.mean()) / series_a.std()
z_b = (series_b - series_b.mean()) / series_b.std()

ax_single.plot(x, z_a, color="tab:blue", label="Series A (standardized)")
ax_single.plot(x, z_b, color="tab:red", linestyle="--", label="Series B (standardized)")
ax_single.set_xlabel("x")
ax_single.set_ylabel("Standardized value (z-score)")
ax_single.set_title("Single Y-Axis (Standardized): Honest Comparison")
ax_single.legend()
ax_single.grid(True, alpha=0.3)

fig.suptitle("Pitfall 5: Dual Y-Axis Can Fabricate Fake Correlation", fontsize=13)
fig.tight_layout(rect=[0, 0, 1, 0.93])

# ------------------------- 3. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "pitfall05_dual_axis_misleading.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")
print(f"实际相关系数 corr(series_a, series_b) = {np.corrcoef(series_a, series_b)[0,1]:.3f}")

plt.show()

# ------------------------- 如何阅读这两张图 -------------------------
# 左图（双 Y 轴）：因为两个 y 轴各自独立缩放，Matplotlib 会自动把每条线的
#      数据范围拉伸到填满各自的绘图区域，视觉上两条线"看起来都在往某个方向走"，
#      容易让人误以为它们高度相关。
# 右图（标准化后单 Y 轴）：把两组数据都转换成 z-score（减均值除以标准差）后
#      画在同一个 y 轴上，这才是公平的比较——可以看到 series_b 其实只是在 0
#      附近随机波动，和 series_a 的稳定上升趋势毫无关系。
#
# 排查建议：任何时候看到双 Y 轴图表"两条线走势惊人一致"，
# 第一反应应该是检查两个 y 轴各自的刻度范围（ax1.get_ylim() / ax2.get_ylim()），
# 确认这种"一致"不是缩放选择造成的视觉巧合。
# 更稳妥的做法：像右图这样标准化后放到同一个 y 轴，或者拆成上下两个独立子图
#（参见 examples/ex06_multiple_subplots.py），而不是依赖双 Y 轴。
