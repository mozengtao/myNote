#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工程案例二：网络性能分析

Network Test
     │
     ├── Throughput
     ├── RTT
     ├── Packet Loss
     │
     └── Correlation
              │
              ▼
        Visualization

输出图（保存到 output/network_analysis/）：
    throughput_timeseries.png   —— 吞吐量时序（标出异常点）
    rtt_timeseries.png          —— RTT 时序（标出异常点）
    packet_loss_timeseries.png  —— 丢包率时序
    throughput_vs_rtt.png       —— throughput vs RTT 散点（相关性探索）
    throughput_distribution.png —— throughput 分布
    rtt_percentiles.png         —— P50/P95/P99 RTT

运行前请先运行 generate_sample_network_csv.py 生成 data/network_test.csv。

这个案例要重点体现：Matplotlib 在系统/网络工程中的实际价值——
把一堆散乱的探测数据变成"能一眼看出什么时候出问题"的图形。
"""

from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def detect_anomalies(df: pd.DataFrame) -> pd.Series:
    """
    异常点判定规则（简单但实用）：
    丢包率显著偏高（> mean + 3*std）即认为该时刻网络出现异常事件。
    这是统计计算（Pandas/NumPy），不是 Matplotlib 的职责。
    """
    loss = df["loss_percent"]
    threshold = loss.mean() + 3 * loss.std()
    return loss > threshold


def main() -> None:
    base_dir = Path(__file__).resolve().parent
    csv_path = base_dir / "data" / "network_test.csv"

    if not csv_path.exists():
        raise FileNotFoundError(
            f"找不到 {csv_path}，请先运行 generate_sample_network_csv.py 生成示例数据。"
        )

    # ------------------------- 1. Load -------------------------
    df = pd.read_csv(csv_path, parse_dates=["timestamp"])

    # ------------------------- 2. Understand -------------------------
    print("=== 数据概览 ===")
    print(df.shape)
    print(df.dtypes)
    print(df.describe())

    # ------------------------- 3. Compute Statistics -------------------------
    df["is_anomaly"] = detect_anomalies(df)

    p50 = np.percentile(df["rtt_ms"], 50)
    p95 = np.percentile(df["rtt_ms"], 95)
    p99 = np.percentile(df["rtt_ms"], 99)

    corr_throughput_rtt = df["throughput_mbps"].corr(df["rtt_ms"])

    anomalies = df[df["is_anomaly"]]
    print(f"\n检测到 {len(anomalies)} 个异常采样点 (丢包率显著偏高)")

    output_dir = base_dir.parent.parent / "output" / "network_analysis"
    output_dir.mkdir(parents=True, exist_ok=True)

    # ------------------------- 4. Visualize -------------------------

    # 4.1 throughput_timeseries.png（标出异常点）
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(df["timestamp"], df["throughput_mbps"], label="Throughput", color="tab:blue")
    ax.scatter(
        anomalies["timestamp"], anomalies["throughput_mbps"],
        color="tab:red", zorder=5, label="Anomaly",
    )
    ax.set_title("Throughput over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Throughput (Mbps)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "throughput_timeseries.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.2 rtt_timeseries.png（标出异常点）
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(df["timestamp"], df["rtt_ms"], label="RTT", color="tab:green")
    ax.scatter(
        anomalies["timestamp"], anomalies["rtt_ms"],
        color="tab:red", zorder=5, label="Anomaly",
    )
    ax.set_title("RTT over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("RTT (ms)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "rtt_timeseries.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.3 packet_loss_timeseries.png
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(df["timestamp"], df["loss_percent"], color="tab:orange")
    ax.axhline(
        df["loss_percent"].mean() + 3 * df["loss_percent"].std(),
        color="tab:red", linestyle="--", label="Anomaly threshold (mean+3std)",
    )
    ax.set_title("Packet Loss over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Loss (%)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "packet_loss_timeseries.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.4 throughput_vs_rtt.png
    fig, ax = plt.subplots(figsize=(7, 6))
    ax.scatter(
        df.loc[~df["is_anomaly"], "throughput_mbps"],
        df.loc[~df["is_anomaly"], "rtt_ms"],
        alpha=0.5, label="Normal",
    )
    ax.scatter(
        anomalies["throughput_mbps"], anomalies["rtt_ms"],
        color="tab:red", label="Anomaly",
    )
    ax.set_title(f"Throughput vs RTT (corr={corr_throughput_rtt:.2f})")
    ax.set_xlabel("Throughput (Mbps)")
    ax.set_ylabel("RTT (ms)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "throughput_vs_rtt.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.5 throughput_distribution.png
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.hist(df["throughput_mbps"], bins=30, edgecolor="black", alpha=0.8)
    ax.set_title("Throughput Distribution")
    ax.set_xlabel("Throughput (Mbps)")
    ax.set_ylabel("Frequency")
    fig.tight_layout()
    fig.savefig(output_dir / "throughput_distribution.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.6 rtt_percentiles.png
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.hist(df["rtt_ms"], bins=30, edgecolor="black", alpha=0.8, color="tab:green")
    ax.axvline(p50, color="tab:blue", linestyle="--", label=f"P50={p50:.1f}ms")
    ax.axvline(p95, color="tab:orange", linestyle="--", label=f"P95={p95:.1f}ms")
    ax.axvline(p99, color="tab:red", linestyle="--", label=f"P99={p99:.1f}ms")
    ax.set_title("RTT Distribution with P50/P95/P99")
    ax.set_xlabel("RTT (ms)")
    ax.set_ylabel("Frequency")
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_dir / "rtt_percentiles.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # ------------------------- 5. Interpret -------------------------
    print("\n=== 分析结论 ===")
    print(f"RTT P50/P95/P99: {p50:.1f} / {p95:.1f} / {p99:.1f} ms")
    print(f"throughput 与 RTT 相关系数: {corr_throughput_rtt:.3f}（负相关：吞吐降低时延迟升高，异常时段的强负相关拉高了整体相关系数）")
    print(f"检测到 {len(anomalies)} 个异常采样点，集中时段：")
    if not anomalies.empty:
        print(anomalies["timestamp"].dt.floor("min").drop_duplicates().to_list())

    print(f"\n6 张图已保存到: {output_dir}")


if __name__ == "__main__":
    main()
