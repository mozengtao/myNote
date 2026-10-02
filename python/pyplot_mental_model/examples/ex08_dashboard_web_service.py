#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 8：真实工程数据分析 —— Web Service 监控 Dashboard

这张 Dashboard 要回答的问题：
    "这个 Web 服务过去 24 小时整体健康吗？"
    拆解成 6 个子问题，对应 6 个子图：
      1. 每小时请求量：流量趋势是否正常？
      2. 每小时 error rate：有没有错误率突增的时段？
      3. latency 分布：大多数请求的延迟集中在哪个范围？
      4. 不同 service 的延迟对比：哪个服务拖慢了整体表现？
      5. request_size vs latency：请求体越大，延迟是否也越高？
      6. （标题栏中额外汇总 P50/P95/P99）

布局（与需求一致的 6 宫格，最后一行合并为一整行）：
    +----------------------+----------------------+
    | Requests / Hour      | Error Rate / Hour    |
    +----------------------+----------------------+
    | Latency Distribution | Service Comparison   |
    +----------------------+----------------------+
    | Request Size vs Latency                    |
    +---------------------------------------------+
"""

from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# ============================================================
# 1. Prepare Data —— 模拟生成 24 小时的 Web 服务访问日志
# ============================================================
np.random.seed(7)

N = 5000  # 总请求数
services = ["auth", "user", "order", "payment", "search"]

# 时间戳：均匀分布在过去 24 小时内
start = pd.Timestamp("2026-09-28T00:00:00")
timestamps = start + pd.to_timedelta(
    np.random.uniform(0, 24 * 3600, size=N), unit="s"
)

service = np.random.choice(services, size=N, p=[0.25, 0.2, 0.25, 0.15, 0.15])

# 不同服务给不同的延迟基线，模拟"payment 服务天生更慢"这种真实场景
latency_base = {
    "auth": 60,
    "user": 80,
    "order": 120,
    "payment": 200,
    "search": 100,
}
latency_ms = np.array(
    [latency_base[s] for s in service]
) + np.random.gamma(shape=2.0, scale=15.0, size=N)

# 请求体大小（字节），并让它与 latency 有一定正相关（越大的请求处理越慢）
request_size = np.random.uniform(200, 5000, size=N)
latency_ms = latency_ms + request_size / 200.0

# 状态码：95% 是 200，其余随机分布在 4xx/5xx
status_code = np.random.choice(
    [200, 400, 404, 500, 503],
    size=N,
    p=[0.95, 0.02, 0.01, 0.015, 0.005],
)

df = pd.DataFrame(
    {
        "timestamp": timestamps,
        "service": service,
        "status_code": status_code,
        "latency_ms": latency_ms,
        "request_size": request_size,
    }
).sort_values("timestamp").reset_index(drop=True)

# ============================================================
# 2. Compute Statistics —— 这一步全部由 NumPy/Pandas 完成，
#    Matplotlib 只负责把下面算出来的结果画出来
# ============================================================
df["hour"] = df["timestamp"].dt.floor("h")
df["is_error"] = df["status_code"] >= 400

requests_per_hour = df.groupby("hour").size()
error_rate_per_hour = df.groupby("hour")["is_error"].mean() * 100  # 百分比

p50 = np.percentile(df["latency_ms"], 50)
p95 = np.percentile(df["latency_ms"], 95)
p99 = np.percentile(df["latency_ms"], 99)

latency_by_service = [
    df.loc[df["service"] == s, "latency_ms"].values for s in services
]

# ============================================================
# 3. Create Figure + GridSpec —— 6 宫格布局，最后一行跨两列
# ============================================================
fig = plt.figure(figsize=(14, 12))
gs = GridSpec(3, 2, figure=fig, height_ratios=[1, 1, 1])

ax_requests = fig.add_subplot(gs[0, 0])
ax_error = fig.add_subplot(gs[0, 1])
ax_latency_dist = fig.add_subplot(gs[1, 0])
ax_service_cmp = fig.add_subplot(gs[1, 1])
ax_size_vs_latency = fig.add_subplot(gs[2, :])  # 最后一行合并为一整行

# ------------------------------------------------------------
# 3.1 Requests / Hour
# ------------------------------------------------------------
ax_requests.plot(requests_per_hour.index, requests_per_hour.values, marker="o")
ax_requests.set_title("Requests / Hour")
ax_requests.set_xlabel("Hour")
ax_requests.set_ylabel("Requests")
ax_requests.tick_params(axis="x", rotation=45)
ax_requests.grid(True, alpha=0.3)

# ------------------------------------------------------------
# 3.2 Error Rate / Hour
# ------------------------------------------------------------
ax_error.plot(
    error_rate_per_hour.index, error_rate_per_hour.values, marker="o", color="tab:red"
)
ax_error.set_title("Error Rate / Hour")
ax_error.set_xlabel("Hour")
ax_error.set_ylabel("Error Rate (%)")
ax_error.tick_params(axis="x", rotation=45)
ax_error.grid(True, alpha=0.3)

# ------------------------------------------------------------
# 3.3 Latency Distribution + P50/P95/P99 标记
# ------------------------------------------------------------
ax_latency_dist.hist(df["latency_ms"], bins=40, edgecolor="black", alpha=0.8)
ax_latency_dist.axvline(p50, color="tab:green", linestyle="--", label=f"P50={p50:.0f}ms")
ax_latency_dist.axvline(p95, color="tab:orange", linestyle="--", label=f"P95={p95:.0f}ms")
ax_latency_dist.axvline(p99, color="tab:red", linestyle="--", label=f"P99={p99:.0f}ms")
ax_latency_dist.set_title("Latency Distribution")
ax_latency_dist.set_xlabel("Latency (ms)")
ax_latency_dist.set_ylabel("Frequency")
ax_latency_dist.legend()

# ------------------------------------------------------------
# 3.4 Service Comparison（boxplot）
# ------------------------------------------------------------
ax_service_cmp.boxplot(latency_by_service, tick_labels=services)
ax_service_cmp.set_title("Latency by Service")
ax_service_cmp.set_xlabel("Service")
ax_service_cmp.set_ylabel("Latency (ms)")
ax_service_cmp.grid(axis="y", alpha=0.3)

# ------------------------------------------------------------
# 3.5 Request Size vs Latency（scatter，整行宽图）
# ------------------------------------------------------------
ax_size_vs_latency.scatter(df["request_size"], df["latency_ms"], alpha=0.3, s=10)
ax_size_vs_latency.set_title("Request Size vs Latency")
ax_size_vs_latency.set_xlabel("Request Size (bytes)")
ax_size_vs_latency.set_ylabel("Latency (ms)")
ax_size_vs_latency.grid(True, alpha=0.3)

# ------------------------------------------------------------
# 4. Layout + Suptitle
# ------------------------------------------------------------
fig.suptitle("Web Service Monitoring Dashboard (last 24h)", fontsize=16, y=1.0)
fig.tight_layout(rect=[0, 0, 1, 0.97])  # 给 suptitle 留出顶部空间

# ------------------------------------------------------------
# 5. Save
# ------------------------------------------------------------
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
out_path = OUTPUT_DIR / "ex08_dashboard_web_service.png"
fig.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"图已保存: {out_path}")
print(f"P50={p50:.1f}ms  P95={p95:.1f}ms  P99={p99:.1f}ms")

plt.show()

# ------------------------------------------------------------
# 如何阅读这个 Dashboard
# ------------------------------------------------------------
# 1. Requests/Hour：先看整体流量是否有明显的高峰/低谷时段。
# 2. Error Rate/Hour：对照同一时间点，流量高峰时错误率是否也升高
#    （可能说明系统在高负载下更容易出错）。
# 3. Latency Distribution：P50/P95/P99 三条竖线把"大多数用户的体验(P50)"
#    和"最差 5%/1% 用户的体验(P95/P99)"同时呈现出来——只看均值会掩盖长尾问题。
# 4. Latency by Service：定位到底是哪个服务在拖慢整体表现（这里是 payment）。
# 5. Request Size vs Latency：验证"请求体越大，延迟越高"这个假设是否成立，
#    但同样要提醒自己：这只是相关性，不是因果性证明。
