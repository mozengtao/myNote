#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 6：多个子图 —— 同时观察多个指标

这张图要回答的问题：
    "requests / latency / errors 这三个指标，随时间的走势是否一致？
     有没有某个指标异常波动，而其他指标没有？"

数据关系：3 个独立的时间序列，共享同一个 x 轴（day）。

核心认知：
    plt.subplots(nrows, ncols) 是复杂绘图的核心入口——
    它一次性创建一个 Figure 和一组 Axes，并把 Axes 以数组形式返回，
    后续用下标 axes[i] 显式操作每一个子图，不依赖"当前 Axes"这种隐式状态。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(0)

days = np.arange(1, 31)

requests = 1000 + 100 * np.sin(days / 4)
latency = 100 + 10 * np.cos(days / 5)
errors = 20 + 5 * np.sin(days / 3)

# ------------------------- 2. Create Figure/Axes -------------------------
# sharex=True: 三个子图共用同一个 x 轴范围/刻度，方便上下对齐比较同一天的数据
fig, axes = plt.subplots(
    3,
    1,
    figsize=(10, 10),
    sharex=True,
)

# ------------------------- 3/4. Plot + Configure 每个子图 -------------------------
axes[0].plot(days, requests)
axes[0].set_ylabel("Requests")
axes[0].set_title("System Metrics")

axes[1].plot(days, latency)
axes[1].set_ylabel("Latency (ms)")

axes[2].plot(days, errors)
axes[2].set_ylabel("Errors")
axes[2].set_xlabel("Day")  # 只在最下面的子图标 x 轴，因为 sharex 已经对齐了

# ------------------------- 5/6. Annotation & Layout -------------------------
for ax in axes:
    ax.grid(True, alpha=0.3)

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex06_multiple_subplots.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex06_multiple_subplots.png'}")

plt.show()

# ------------------------- 对象关系 -------------------------
# Figure
#  │
#  ├── Axes[0]  (requests)
#  ├── Axes[1]  (latency)
#  └── Axes[2]  (errors)
#
# 三个 Axes 属于同一个 Figure，因此可以用 fig.tight_layout() 统一调整布局，
# 也可以用 sharex=True 让它们共享同一套 x 轴刻度和缩放/平移行为。
#
# ------------------------- 如何阅读这张图 -------------------------
# 上下对齐同一个 x 坐标（同一天），观察三条曲线是否"同时"出现异常波动——
# 例如某一天 errors 突然升高，同时 latency 是否也在升高，
# 这种"跨指标对齐观察"正是多子图（而不是三张独立的图）的核心价值。
