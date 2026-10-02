#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
误读 2：Histogram bins 数量的选择

同一份数据，用 bins=5 / bins=20 / bins=100 画三张直方图，
对比视觉差异，理解 bins 太少/太多分别会掩盖什么信息。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(1)

# 构造一个"双峰分布"：两组不同均值的正态分布叠加
data = np.concatenate(
    [
        np.random.normal(90, 8, 500),
        np.random.normal(140, 10, 500),
    ]
)

bin_settings = [5, 20, 100]

# ------------------------- 2. Create Figure/Axes -------------------------
fig, axes = plt.subplots(1, 3, figsize=(15, 5), sharey=False)

# ------------------------- 3/4. 逐个 bins 设置画图 -------------------------
for ax, bins in zip(axes, bin_settings):
    ax.hist(data, bins=bins, edgecolor="black", color="tab:blue", alpha=0.8)
    ax.set_title(f"bins={bins}")
    ax.set_xlabel("Value")
    ax.set_ylabel("Frequency")
    ax.grid(True, alpha=0.3)

fig.suptitle("Pitfall 2: Histogram bins Changes What You See", fontsize=14)
fig.tight_layout(rect=[0, 0, 1, 0.94])

# ------------------------- 5. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "pitfall02_histogram_bins.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")

plt.show()

# ------------------------- 如何阅读这三张图 -------------------------
# bins=5  ：区间太宽，两个峰被合并抹平成一个"矮胖"的柱子，
#           完全看不出这其实是"双峰分布"（数据里有两个不同的群体）。
# bins=20 ：能看出明显的双峰形状，是这份数据比较合适的 bins 数量。
# bins=100：区间太窄，每个柱子里的样本数太少，图变得锯齿状、充满随机噪声，
#           反而干扰了对整体形状的判断。
#
# 结论：没有"绝对正确"的 bins 数量，但一个实用的经验法则是：
# 先用一个中等值（比如 sqrt(样本数) 或 20~50）观察大致形状，
# 再根据数据特点（是否需要看清多峰、细节）适当调整，
# 并且永远要意识到"bins 选择本身会改变你看到的分布形状"。
