#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
统计量可视化：把 mean/median/std/IQR/percentile 叠加到分布图上

这张图要回答的问题：
    "这批延迟数据的'中心'在哪？'典型波动范围'有多大？'最坏情况'有多差？"

数据流（必须牢记这条单向链路）：
    NumPy / Pandas（统计计算）
          │
          ▼
      Statistics（统计量：mean/median/std/percentile/IQR）
          │
          ▼
      Matplotlib（可视化：ax.axvline 把数字变成一条竖线）
          │
          ▼
      Human interpretation（人从图上读出"多数用户体验 vs 最差 5% 体验"）

!! Matplotlib 本身不做任何统计计算 !!
上面的 mean/median/percentile 全部由 numpy 算好，
matplotlib 只负责把算好的数字，画成一条可以被人眼定位的竖线。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(42)

# 模拟一批带轻微长尾的延迟数据：主体是正态分布，叠加一小部分慢请求
latency = np.concatenate(
    [
        np.random.normal(100, 15, 950),
        np.random.normal(220, 30, 50),  # 少数"慢请求"，制造长尾
    ]
)

# ------------------------- 2. Compute Statistics（全部由 NumPy 完成） -------------------------
mean = np.mean(latency)
median = np.median(latency)
std = np.std(latency)
variance = np.var(latency)

q1 = np.percentile(latency, 25)
q3 = np.percentile(latency, 75)
iqr = q3 - q1

p95 = np.percentile(latency, 95)
p99 = np.percentile(latency, 99)

data_min = np.min(latency)
data_max = np.max(latency)

print("=== 统计量（由 NumPy 计算，Matplotlib 不参与计算） ===")
print(f"mean     = {mean:.2f}")
print(f"median   = {median:.2f}")
print(f"std      = {std:.2f}")
print(f"variance = {variance:.2f}")
print(f"Q1/Q3    = {q1:.2f} / {q3:.2f}  (IQR={iqr:.2f})")
print(f"P95/P99  = {p95:.2f} / {p99:.2f}")
print(f"min/max  = {data_min:.2f} / {data_max:.2f}")

# ------------------------- 3. Create Figure/Axes + Plot -------------------------
fig, ax = plt.subplots(figsize=(10, 6))

ax.hist(latency, bins=40, edgecolor="black", alpha=0.75, color="tab:blue")

# ------------------------- 4. 把统计量叠加成可视化标记（axvline） -------------------------
# center：mean 和 median 用于定位"典型值"，两者不同说明分布不对称（有长尾）
ax.axvline(mean, color="tab:red", linestyle="-", linewidth=2, label=f"mean={mean:.0f}")
ax.axvline(median, color="tab:green", linestyle="-", linewidth=2, label=f"median={median:.0f}")

# spread：用 IQR 的两端（Q1/Q3）标出"中间 50% 数据"的范围
ax.axvline(q1, color="tab:gray", linestyle="--", linewidth=1.5, label=f"Q1={q1:.0f}")
ax.axvline(q3, color="tab:gray", linestyle="--", linewidth=1.5, label=f"Q3={q3:.0f}")

# tail：P95/P99 标出"最差 5%/1% 用户"的体验下限
ax.axvline(p95, color="tab:orange", linestyle=":", linewidth=2, label=f"P95={p95:.0f}")
ax.axvline(p99, color="tab:purple", linestyle=":", linewidth=2, label=f"P99={p99:.0f}")

ax.set_title("Latency Distribution with Statistical Markers")
ax.set_xlabel("Latency (ms)")
ax.set_ylabel("Frequency")
ax.legend(ncol=2, fontsize=9)
ax.grid(True, alpha=0.3)

fig.tight_layout()

# ------------------------- 5. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "distribution_stats_overlay.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")

plt.show()

# ------------------------- 如何阅读这张图 -------------------------
# Distribution
#      │
#      ├── center
#      │     ├── mean   —— 受极端值影响较大（这里因为长尾请求，mean 会被拉高）
#      │     └── median —— 对极端值更稳健，更能代表"典型用户"的体验
#      │
#      ├── spread
#      │     ├── std    —— 全局波动程度，同样容易被极端值放大
#      │     └── IQR    —— 只看中间 50% 数据的波动，更稳健
#      │
#      └── tail
#            └── P95/P99 —— 直接回答"最差情况有多差"，这是均值永远无法回答的问题
#
# 观察重点：mean 明显高于 median，说明数据右侧有长尾（少数慢请求把均值"拉高"了）。
# 这正是为什么线上系统监控通常用 P95/P99 而不是 mean 作为 SLA 指标——
# mean 会被少数极端值稀释，掩盖"最差用户体验有多差"这个真正重要的问题。
