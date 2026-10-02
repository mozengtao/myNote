#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
误读 4：Correlation != Causation（相关性不是因果性）

构造一个"共同原因"场景：
    真实的因是"并发请求数(concurrency)"，
    它同时推高了 cpu_usage 和 latency，
    如果只画 cpu_usage vs latency 的散点图，会看起来"cpu 导致了 latency"，
    但真正的原因是背后的第三个变量 concurrency。
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------- 1. Prepare Data -------------------------
np.random.seed(3)

n = 300
concurrency = np.random.uniform(10, 200, n)  # 真正的驱动因素：并发请求数

# cpu_usage 和 latency 都由 concurrency 驱动，而不是彼此驱动
cpu_usage = 10 + 0.35 * concurrency + np.random.normal(0, 5, n)
latency = 30 + 0.9 * concurrency + np.random.normal(0, 15, n)

corr_coef = np.corrcoef(cpu_usage, latency)[0, 1]

# ------------------------- 2. Create Figure/Axes -------------------------
fig, (ax_naive, ax_true_driver) = plt.subplots(1, 2, figsize=(13, 5.5))

# ------------------------- 3/4. 左图：只看 cpu vs latency（容易被误读为因果） -------------------------
ax_naive.scatter(cpu_usage, latency, alpha=0.6, color="tab:blue")
ax_naive.set_title(f"CPU vs Latency  (corr={corr_coef:.2f})")
ax_naive.set_xlabel("CPU Usage (%)")
ax_naive.set_ylabel("Latency (ms)")
ax_naive.grid(True, alpha=0.3)
ax_naive.text(
    0.05, 0.95,
    "Looks like CPU causes Latency...\nbut this is only correlation!",
    transform=ax_naive.transAxes,
    va="top", fontsize=10, color="tab:red",
)

# ------------------------- 3/4. 右图：加入真正的驱动因素 concurrency（用颜色编码） -------------------------
sc = ax_true_driver.scatter(
    cpu_usage, latency, c=concurrency, cmap="viridis", alpha=0.8
)
ax_true_driver.set_title("Same Data, Colored by True Driver: Concurrency")
ax_true_driver.set_xlabel("CPU Usage (%)")
ax_true_driver.set_ylabel("Latency (ms)")
ax_true_driver.grid(True, alpha=0.3)
cbar = fig.colorbar(sc, ax=ax_true_driver)
cbar.set_label("Concurrency")

fig.suptitle("Pitfall 4: Correlation != Causation", fontsize=14)
fig.tight_layout(rect=[0, 0, 1, 0.93])

# ------------------------- 5. Save/Show -------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "pitfall04_correlation_not_causation.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")
print(f"corr(cpu_usage, latency) = {corr_coef:.3f}")

plt.show()

# ------------------------- 如何阅读这两张图 -------------------------
# 左图：cpu_usage 和 latency 呈现明显正相关（corr 系数很高），
#      如果止步于此，很容易得出"降低 CPU 使用率就能降低延迟"的结论。
# 右图：用颜色把"真正的驱动因素" concurrency（并发请求数）编码出来，
#      可以看到颜色（concurrency）沿着同一个方向变化，
#      暗示 cpu_usage 和 latency 可能是被同一个第三方因素共同驱动的，
#      而不是彼此因果。
#
# 结论：散点图 + 相关系数只能告诉你"两个变量一起变化的程度"，
# 永远不能单独用来证明"谁导致了谁"。要验证因果关系，
# 需要受控实验（A/B test）或更严格的因果推断方法，
# 而不是仅凭一张 scatter plot。
