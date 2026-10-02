#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 3：Histogram —— 分布分析

这张图要回答的问题：
    "HTTP 延迟大概集中在什么范围？有没有长尾？分布是否对称？"

数据关系：单变量分布（只有一列 latency 数值）。
按照 03_chart_selection.md 的决策树：分布 -> histogram。

核心链路：
    Raw Samples（原始样本）
        ↓  按数值范围切成若干区间(bins)
    bins
        ↓  统计每个区间里落入多少个样本
    frequency（频数）
        ↓
    distribution（分布形状：中心、分散程度、偏态）
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(42)

# 模拟 1000 次 HTTP 请求的延迟（毫秒），均值 100ms，标准差 20ms
latency = np.random.normal(
    loc=100,
    scale=20,
    size=1000,
)

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax = plt.subplots(figsize=(9, 5))

# ------------------------- 3. Plot Data -------------------------
ax.hist(
    latency,
    bins=30,           # 把数据范围切成 30 个等宽区间
    edgecolor="black",  # 给每个柱子加黑边，方便区分相邻的柱子
)

# ------------------------- 4. Configure Axes -------------------------
ax.set_title("HTTP Latency Distribution")
ax.set_xlabel("Latency (ms)")
ax.set_ylabel("Frequency")

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex03_histogram_distribution.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex03_histogram_distribution.png'}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# 1. x 轴是延迟值的区间，y 轴是落在该区间内的样本数量（频数）。
# 2. bins 数量的影响（详见 07_misleading_charts.md / pitfalls/pitfall02_histogram_bins.py）：
#      - bins 太少：多个真实的小峰会被"抹平"合并成一个大柱子，丢失细节；
#      - bins 太多：每个柱子样本量太少，图会变得"锯齿状"、充满噪声，
#        反而看不出整体分布形状。
# 3. 分布的三个关键特征：
#      - center（中心）：柱子最高的地方大致对应 mean/median；
#      - spread（分散程度）：柱子分布的宽度，对应 std/IQR；
#      - skew（偏态）：柱子是否左右对称，还是一侧有更长的"尾巴"。
