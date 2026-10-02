#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 4：Scatter —— 两变量关系

这张图要回答的问题：
    "CPU 使用率和延迟之间有没有关系？关系有多强？有没有异常点？"

数据关系：每个观测（一次采样）同时有两个数值属性：cpu 和 latency。
按照 03_chart_selection.md 的决策树：两变量关系 -> scatter。

变量角色：
    x = cpu      -> independent / explanatory variable（自变量/解释变量）
    y = latency  -> dependent / response variable（因变量/响应变量）
    每个点        -> one observation（一次真实的观测）
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(42)

cpu = np.random.uniform(10, 90, 200)

# 人为构造一个"cpu 越高、延迟越高"的线性关系 + 随机噪声
latency = (
    20
    + 0.8 * cpu
    + np.random.normal(0, 10, 200)
)

# ------------------------- 2. Create Figure/Axes -------------------------
fig, ax = plt.subplots(figsize=(8, 6))

# ------------------------- 3. Plot Data -------------------------
# alpha < 1 让重叠的点变深色，间接反映"点的密度"，
# 在点数较多时比不透明散点更容易看出聚集区域。
ax.scatter(cpu, latency, alpha=0.6)

# ------------------------- 4. Configure Axes -------------------------
ax.set_title("CPU Usage vs Latency")
ax.set_xlabel("CPU Usage (%)")
ax.set_ylabel("Latency (ms)")

ax.grid(True, alpha=0.3)

fig.tight_layout()

# ------------------------- 7/8. Render & Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
fig.savefig(OUTPUT_DIR / "ex04_scatter_relationship.png", dpi=150, bbox_inches="tight")
print(f"图已保存: {OUTPUT_DIR / 'ex04_scatter_relationship.png'}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# 1. correlation（相关性）：点是否呈现出"从左下到右上"或"从左上到右下"的
#    整体走向。这里我们人为构造了正相关，所以能看到明显的上升趋势。
# 2. outlier（异常点）：远离主体点云的孤立点，可能代表异常事件，值得单独排查。
# 3. cluster（聚集）：如果点明显分成几团，可能说明数据里混杂了不同的"子群体"
#    （比如不同机器型号、不同时间段），需要拆开单独分析。
# 4. trend（趋势）：整体的线性/非线性走向。
#
# !! 重要提醒 !!
# 这张图只能说明 cpu 和 latency "统计上相关"，不能说明"cpu 升高导致了延迟升高"。
# Correlation != Causation（相关性不是因果性）——
# 也许是第三个因素（比如并发请求数）同时导致了 cpu 和 latency 都升高。
# 完整讨论见 07_misleading_charts.md 与 pitfalls/pitfall04_correlation_not_causation.py。
