#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
误读 3：Outlier 对 mean 的影响（mean vs median）

生成一批"正常数据"，先画出 mean/median；
再加入几个极端 outlier，看 mean 如何被大幅拉偏，而 median 几乎不变。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(2)

normal_data = np.random.normal(100, 10, 200)

# 加入 3 个极端 outlier（例如一次异常的超时请求，延迟高达 2000ms）
outliers = np.array([2000, 1800, 2200])
data_with_outliers = np.concatenate([normal_data, outliers])

# ------------------------- 2. Compute Statistics -------------------------
mean_before = np.mean(normal_data)
median_before = np.median(normal_data)

mean_after = np.mean(data_with_outliers)
median_after = np.median(data_with_outliers)

print("=== 加入 outlier 前后对比 ===")
print(f"加入前: mean={mean_before:.1f}, median={median_before:.1f}")
print(f"加入后: mean={mean_after:.1f}, median={median_after:.1f}")
print(f"mean 变化: {mean_after - mean_before:+.1f}")
print(f"median 变化: {median_after - median_before:+.1f}")

# ------------------------- 3. Create Figure/Axes + Plot -------------------------
fig, (ax_before, ax_after) = plt.subplots(1, 2, figsize=(13, 5), sharey=True)

# 左图：不含 outlier
ax_before.hist(normal_data, bins=25, edgecolor="black", alpha=0.75, color="tab:blue")
ax_before.axvline(mean_before, color="tab:red", linewidth=2, label=f"mean={mean_before:.0f}")
ax_before.axvline(median_before, color="tab:green", linewidth=2, label=f"median={median_before:.0f}")
ax_before.set_title("Without Outliers")
ax_before.set_xlabel("Latency (ms)")
ax_before.set_ylabel("Frequency")
ax_before.legend()

# 右图：含 outlier（注意 x 轴范围会被极端值拉得很宽）
ax_after.hist(data_with_outliers, bins=25, edgecolor="black", alpha=0.75, color="tab:orange")
ax_after.axvline(mean_after, color="tab:red", linewidth=2, label=f"mean={mean_after:.0f}")
ax_after.axvline(median_after, color="tab:green", linewidth=2, label=f"median={median_after:.0f}")
ax_after.set_title("With 3 Extreme Outliers")
ax_after.set_xlabel("Latency (ms)")
ax_after.legend()

fig.suptitle("Pitfall 3: Outliers Distort mean, median Stays Robust", fontsize=14)
fig.tight_layout(rect=[0, 0, 1, 0.94])

# ------------------------- 4. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "pitfall03_outlier_mean_vs_median.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")

plt.show()

# ------------------------- 如何阅读这两张图 -------------------------
# 左图：200 个正常样本，mean 和 median 几乎重合，因为数据近似对称、没有极端值。
# 右图：仅加入 3 个极端 outlier（占比 1.5%），mean 就被大幅拉高
#      （因为 mean 的计算会把每个值都纳入总和，极端值的影响是"全局性"的），
#      而 median 几乎不变（因为 median 只关心"排序后正中间的那个值"，
#      3 个极端值不管多极端，都只是排到了最后，不影响中间位置）。
#
# 工程启示：如果你的监控指标用的是 mean latency，
# 一次偶发的严重超时就可能让这个指标"看起来"整体变差，
# 但实际上绝大多数用户毫无感觉——这正是为什么应该同时看 median 和 P95/P99
# （参见 06_statistics_and_viz.md），而不能只看一个 mean 数字。
