#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 1：Line Plot —— 时间序列

这张图要回答的问题：
    "过去 30 天，每天的流量是怎么变化的？有没有周期性/趋势？"

数据关系：x 轴是有序的时间（day），y 轴是随时间变化的数值（traffic）。
按照 03_chart_selection.md 的决策树：时间序列 -> line plot。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
# 固定随机种子，保证每次运行结果可复现，方便对照讲解
np.random.seed(0)

days = np.arange(1, 31)

# 用一个 sin 波模拟"周期性波动" + 正态噪声模拟"随机抖动"
traffic = (
    100
    + 20 * np.sin(days / 3)
    + np.random.normal(0, 8, size=len(days))
)

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax = plt.subplots(figsize=(10, 5))

# ------------------------- 3. Plot Data -------------------------
ax.plot(
    days,
    traffic,
    marker="o",       # 每个数据点画一个圆点，方便看清"到底有多少个观测"
    linewidth=1.5,     # 线宽，纯粹是视觉样式
    label="Traffic",   # 提供给 legend() 使用的名字
)

# ------------------------- 4. Configure Axes -------------------------
ax.set_title("Daily Traffic")
ax.set_xlabel("Day")
ax.set_ylabel("Requests")

# ------------------------- 5/6. Annotation & Layout -------------------------
ax.grid(True, alpha=0.3)  # alpha 控制网格线透明度，太重会抢视觉焦点
ax.legend()

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex01_line_plot_timeseries.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex01_line_plot_timeseries.png'}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# 1. x 轴是天数，y 轴是请求量，观察折线的整体走势（是否上升/下降/周期波动）。
# 2. marker="o" 让每个真实观测点清晰可辨，避免"折线看起来很光滑"造成的误导
#    （用户可能误以为数据是连续采样，而实际上只有 30 个离散观测点）。
# 3. Line Plot 的核心任务：观察趋势（trend）、变化（change）和时间关系，
#    不适合用来精确比较"某一天和另一天差多少"（那是 bar 或数值本身该做的事）。
