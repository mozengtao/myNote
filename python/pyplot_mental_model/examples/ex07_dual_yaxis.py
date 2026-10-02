#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 7：双 Y 轴 —— 在同一张图里对比两个不同量级/单位的指标

这张图要回答的问题：
    "requests 和 error_rate 是否同时变化？"（把两个单位、量级都不同的指标
    画在同一张图里，靠"同一个 x 轴"对齐观察它们的走势关系）

对象关系：
    One Figure
        │
        └── shared X (两个 Axes 共享同一个 x 轴范围)
              │
              ├── Axes 1 → Y1 (requests)
              │
              └── Axes 2 → Y2 (error_rate)，通过 ax1.twinx() 创建

!! 重要提醒 !!
    双 Y 轴很容易造成误读：两条线"看起来重合/交叉"，
    完全可能只是因为两个 y 轴的缩放比例被人为选择导致的视觉巧合，
    并不代表两个指标真的有强关联。使用前务必想清楚：
    这张图是不是必须要用双 Y 轴，还是应该拆成两张独立的子图更诚实。
    完整讨论见 07_misleading_charts.md 与 pitfalls/pitfall05_dual_axis_misleading.py。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(0)

days = np.arange(1, 31)

requests = 1000 + 100 * np.sin(days / 4)
error_rate = 2 + 0.5 * np.cos(days / 5)

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax1 = plt.subplots(figsize=(10, 5))

# ------------------------- 3/4. Plot + Configure：第一个 Y 轴 -------------------------
ax1.plot(
    days,
    requests,
    label="Requests",
)

ax1.set_xlabel("Day")
ax1.set_ylabel("Requests")

# ------------------------- 创建第二个 Y 轴（共享同一个 x 轴） -------------------------
ax2 = ax1.twinx()

ax2.plot(
    days,
    error_rate,
    linestyle="--",   # 用不同线型区分两条线，弱化"看起来是同一种数据"的错觉
    label="Error Rate",
)

ax2.set_ylabel("Error Rate (%)")

ax1.set_title("Requests and Error Rate")

# ------------------------- 图例：手动合并两个 Axes 各自的 label -------------------------
# 注意：ax1.legend() 默认只会收集 ax1 上的线，不会自动包含 ax2 的线，
# 因为 ax1 和 ax2 是两个独立的 Axes 对象，各自维护自己的 artist 列表。
lines_1, labels_1 = ax1.get_legend_handles_labels()
lines_2, labels_2 = ax2.get_legend_handles_labels()
ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc="upper left")

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex07_dual_yaxis.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex07_dual_yaxis.png'}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# 1. 两条线共享同一个 x 轴（day），但各自有独立的 y 轴刻度（左：Requests，右：Error Rate）。
# 2. 只能观察"两者是否在同一时间点出现波动/趋势变化"这种粗略的时间对齐关系，
#    不能通过"两条线的高度谁更高"来比较 Requests 和 Error Rate 的绝对大小——
#    它们的刻度完全独立，视觉上的高低毫无可比性。
# 3. 什么时候该用双 Y 轴：两个指标单位/量级差异很大，
#    但你确实需要在同一张图里对齐观察它们随同一个 x 变化的走势（而不是比较大小）。
# 4. 什么时候应该避免：如果只是想让图"看起来好看"或制造"两者强相关"的印象，
#    优先考虑拆成 06 示例那样的多个子图（各自独立 y 轴，更诚实）。
