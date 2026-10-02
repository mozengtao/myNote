#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
误读 1：Y 轴从 0 开始 vs 不从 0 开始（bar chart 的 baseline 陷阱）

同一份数据，画两张 bar chart：
    左：y 轴从 0 开始（诚实的默认行为）
    右：y 轴从接近数据最小值开始（人为放大差异）

结论：bar chart 依赖"柱子高度"传递"数值大小"这个信息，
一旦 baseline 不是 0，柱子高度比例就不再等于数值比例，视觉判断会被放大误导。
"""

from pathlib import Path

import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
services = ["auth", "user", "order", "payment", "search"]
latency_ms = [98, 101, 103, 99, 100]  # 5 个服务的延迟其实非常接近

# ------------------------- 2. Create Figure/Axes -------------------------
fig, (ax_correct, ax_misleading) = plt.subplots(1, 2, figsize=(12, 5))

# ------------------------- 3/4. 左图：baseline = 0（诚实） -------------------------
ax_correct.bar(services, latency_ms, color="tab:blue")
ax_correct.set_ylim(0, max(latency_ms) * 1.2)
ax_correct.set_title("Baseline = 0 (Honest)")
ax_correct.set_xlabel("Service")
ax_correct.set_ylabel("Latency (ms)")
ax_correct.grid(axis="y", alpha=0.3)

# ------------------------- 3/4. 右图：baseline 接近最小值（误导） -------------------------
ax_misleading.bar(services, latency_ms, color="tab:red")
ax_misleading.set_ylim(95, max(latency_ms) + 1)  # 人为收窄 y 轴范围
ax_misleading.set_title("Baseline != 0 (Misleading)")
ax_misleading.set_xlabel("Service")
ax_misleading.set_ylabel("Latency (ms)")
ax_misleading.grid(axis="y", alpha=0.3)

fig.suptitle("Pitfall 1: Bar Chart Baseline Matters", fontsize=14)
fig.tight_layout(rect=[0, 0, 1, 0.94])

# ------------------------- 5. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "pitfall01_bar_baseline.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")

plt.show()

# ------------------------- 如何阅读这两张图 -------------------------
# 左图：5 根柱子高度几乎一样高，如实反映"5 个服务延迟其实很接近"这个事实。
# 右图：因为 y 轴从 95 开始（而不是 0），同样的数据被拉伸得"order 明显比 payment 高很多"，
#      但实际差距只有 4ms，占比不到 4%——右图制造了一种"差距很大"的错误印象。
#
# 排查建议：看到任何 bar chart，第一件事就是检查 y 轴范围（ax.get_ylim()），
# 尤其当"差异看起来很戏剧化"的时候。
