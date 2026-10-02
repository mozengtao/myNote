#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 2：Bar Chart —— 类别比较

这张图要回答的问题：
    "哪个服务的请求量最高？各服务之间差多少？"

数据关系：Category（服务名） -> X 轴；Value（请求量） -> Y 轴。
按照 03_chart_selection.md 的决策树：类别比较 -> bar chart。

为什么不用 line：
    服务名之间没有"顺序"和"连续性"（auth 和 user 之间没有第 1.5 个服务），
    用折线会暗示一种不存在的连续过渡关系，容易误导读者。
"""

from pathlib import Path

import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
services = [
    "auth",
    "user",
    "order",
    "payment",
    "search",
]

requests = [
    1200,
    900,
    1500,
    700,
    1100,
]

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax = plt.subplots(figsize=(9, 5))

# ------------------------- 3. Plot Data -------------------------
ax.bar(services, requests)

# ------------------------- 4. Configure Axes -------------------------
ax.set_title("Requests by Service")
ax.set_xlabel("Service")
ax.set_ylabel("Requests")

# ------------------------- 5/6. Annotation & Layout -------------------------
# axis="y" 表示只画水平网格线；bar chart 主要关心"高度"的比较，
# 水平网格线能帮助视线对齐到 y 轴刻度，垂直网格线反而没什么用。
ax.grid(axis="y", alpha=0.3)

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex02_bar_chart_category.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex02_bar_chart_category.png'}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# 1. 每根柱子的高度代表对应服务的请求量，柱子之间没有顺序含义，
#    调换 services 的顺序不会改变图的"正确性"（但可能影响可读性，
#    比如按数值大小排序会更容易一眼看出排名）。
# 2. 注意 Y 轴是否从 0 开始 —— 这张图默认从 0 开始，
#    如果不从 0 开始会夸大柱子之间的相对差异（详见 07_misleading_charts.md）。
