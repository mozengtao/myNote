#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 5：Boxplot —— 多组分布比较

这张图要回答的问题：
    "auth / order / search 三个服务，延迟分布分别是什么样？
     哪个服务不仅延迟更高，波动也更大？"

数据关系：多组分布比较（3 组独立的延迟样本）。
按照 03_chart_selection.md 的决策树：多组分布比较 -> boxplot。

核心认知：
    Boxplot 是用来比较多个 distribution 的工具，而不仅仅是"画一个箱子"。
    一根箱线图完整编码了 5 个统计量：min, Q1, median, Q3, max（以及异常点）。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(42)

# 构造 service A/B/C latency：不同 均值(loc) 和 波动程度(scale)
data = [
    np.random.normal(100, 10, 200),   # auth：均值低，波动小
    np.random.normal(150, 25, 200),   # order：均值高，波动大
    np.random.normal(120, 15, 200),   # search：介于两者之间
]

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax = plt.subplots(figsize=(8, 5))

# ------------------------- 3. Plot Data -------------------------
# 注: Matplotlib 3.9 起 boxplot() 的 labels 参数改名为 tick_labels
#     （3.11 起旧名会被移除），本教程按新写法演示。
ax.boxplot(
    data,
    tick_labels=["auth", "order", "search"],
)

# ------------------------- 4. Configure Axes -------------------------
ax.set_title("Latency Distribution by Service")
ax.set_xlabel("Service")
ax.set_ylabel("Latency (ms)")

ax.grid(axis="y", alpha=0.3)

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex05_boxplot_multigroup.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex05_boxplot_multigroup.png'}")

plt.show()

# ------------------------- 如何阅读这张箱线图 -------------------------
# 一根箱线图从下到上依次是：
#   - 下方"胡须"(whisker) 末端：通常是 Q1 - 1.5*IQR 范围内的最小值
#   - 箱子下边缘：Q1（第 25 百分位数）
#   - 箱子中间的横线：median（第 50 百分位数，即中位数）
#   - 箱子上边缘：Q3（第 75 百分位数）
#   - 上方"胡须"末端：通常是 Q3 + 1.5*IQR 范围内的最大值
#   - 箱子外的散点（若有）：outliers（超出胡须范围的异常点）
#
# 其中 IQR = Q3 - Q1（Interquartile Range，四分位距），衡量"中间 50% 数据"
# 的分散程度，比 std 更不容易被极端值影响。
#
# 阅读顺序：
#   1. 先比较三个箱子的 median 位置（谁的中心延迟更高）；
#   2. 再比较箱子的高度（IQR，谁的波动更大）；
#   3. 最后看有没有明显的异常点（是否有个别请求延迟远超正常范围）。
