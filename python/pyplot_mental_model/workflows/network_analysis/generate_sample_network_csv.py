#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成模拟的网络性能测试数据：data/network_test.csv

CSV 字段：
    timestamp,throughput_mbps,rtt_ms,loss_percent

用途：给 analyze_network.py 提供一份可复现的输入数据，
     模拟真实网络测试场景（例如周期性带宽/延迟探测）。
"""

from pathlib import Path

import numpy as np
import pandas as pd

np.random.seed(21)

N = 720  # 每 5 秒一个采样点，共 1 小时 (720 * 5s = 3600s)
START_TIME = pd.Timestamp("2026-09-28T00:00:00")

t = np.arange(N)
timestamps = START_TIME + pd.to_timedelta(t * 5, unit="s")

# throughput：基线 800Mbps，叠加慢周期波动 + 噪声
throughput = 800 + 50 * np.sin(t / 40) + np.random.normal(0, 15, N)

# RTT 与 throughput 存在弱负相关：吞吐量下降时延迟略微上升，模拟网络拥塞
rtt = 20 - 0.01 * (throughput - 800) + np.random.normal(0, 2, N)
rtt = np.clip(rtt, 5, None)

# packet loss：大部分时间接近 0，偶尔出现丢包尖峰
loss = np.abs(np.random.normal(0, 0.3, N))

# 人为制造 2 段网络异常事件：吞吐骤降 + RTT 骤升 + 丢包升高
for start, end in [(200, 220), (500, 515)]:
    throughput[start:end] -= np.random.uniform(300, 500, end - start)
    rtt[start:end] += np.random.uniform(30, 80, end - start)
    loss[start:end] += np.random.uniform(3, 8, end - start)

throughput = np.clip(throughput, 0, None)

df = pd.DataFrame(
    {
        "timestamp": timestamps.strftime("%Y-%m-%dT%H:%M:%S"),
        "throughput_mbps": np.round(throughput, 2),
        "rtt_ms": np.round(rtt, 2),
        "loss_percent": np.round(loss, 3),
    }
)

out_dir = Path(__file__).resolve().parent / "data"
out_dir.mkdir(parents=True, exist_ok=True)
out_path = out_dir / "network_test.csv"
df.to_csv(out_path, index=False)

print(f"已生成 {len(df)} 条网络测试记录: {out_path}")
print(df.head())
