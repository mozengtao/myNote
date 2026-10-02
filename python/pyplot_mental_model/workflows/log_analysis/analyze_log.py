#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工程案例一：Web 服务日志分析

完整流程：
    Log 文件
      ↓  正则解析
    Parse
      ↓
    DataFrame
      ↓  groupby / percentile
    Statistics
      ↓
    Matplotlib
      ↓
    PNG report

输出 5 张图（保存到 output/log_analysis/）：
    requests_per_minute.png   —— 每分钟请求量
    error_rate.png            —— 每分钟 error rate
    latency_distribution.png  —— latency 分布
    latency_percentiles.png   —— P50/P95/P99 latency 随时间变化
    service_comparison.png    —— 不同 service 的 latency 对比

运行前请先运行 generate_sample_log.py 生成 data/application.log。
"""

import re
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

LOG_PATTERN = re.compile(
    r"^(?P<timestamp>\S+)\s+"
    r"service=(?P<service>\S+)\s+"
    r"latency=(?P<latency>\d+)\s+"
    r"status=(?P<status>\d+)$"
)


def parse_log(log_path: Path) -> pd.DataFrame:
    """把 application.log 解析成结构化 DataFrame。"""
    rows = []
    with log_path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            m = LOG_PATTERN.match(line)
            if not m:
                # 遇到解析不了的行不要静默丢弃，打印出来方便排查（对应 08_failure_and_debug.md 的原则：
                # 数据问题优先在数据层排查，而不是留到画图阶段才发现）
                print(f"[WARN] 无法解析的日志行: {line}")
                continue
            rows.append(m.groupdict())

    df = pd.DataFrame(rows)
    df["timestamp"] = pd.to_datetime(df["timestamp"])
    df["latency"] = df["latency"].astype(int)
    df["status"] = df["status"].astype(int)
    df["is_error"] = df["status"] >= 400
    return df


def main() -> None:
    base_dir = Path(__file__).resolve().parent
    log_path = base_dir / "data" / "application.log"

    if not log_path.exists():
        raise FileNotFoundError(
            f"找不到 {log_path}，请先运行 generate_sample_log.py 生成示例日志。"
        )

    # ------------------------- 1. Load & Parse -------------------------
    df = parse_log(log_path)

    # ------------------------- 2. Understand -------------------------
    print("=== 数据概览 ===")
    print(df.shape)
    print(df.dtypes)
    print(df.head())
    print(df["latency"].describe())

    # ------------------------- 3. Compute Statistics -------------------------
    df["minute"] = df["timestamp"].dt.floor("min")

    requests_per_minute = df.groupby("minute").size()
    error_rate_per_minute = df.groupby("minute")["is_error"].mean() * 100

    p50_per_minute = df.groupby("minute")["latency"].quantile(0.50)
    p95_per_minute = df.groupby("minute")["latency"].quantile(0.95)
    p99_per_minute = df.groupby("minute")["latency"].quantile(0.99)

    overall_p50 = np.percentile(df["latency"], 50)
    overall_p95 = np.percentile(df["latency"], 95)
    overall_p99 = np.percentile(df["latency"], 99)

    services = sorted(df["service"].unique())
    latency_by_service = [df.loc[df["service"] == s, "latency"].values for s in services]

    output_dir = base_dir.parent.parent / "output" / "log_analysis"
    output_dir.mkdir(parents=True, exist_ok=True)

    # ------------------------- 4. Visualize -------------------------

    # 4.1 requests_per_minute.png
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(requests_per_minute.index, requests_per_minute.values)
    ax.set_title("Requests per Minute")
    ax.set_xlabel("Time")
    ax.set_ylabel("Requests")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "requests_per_minute.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.2 error_rate.png
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(error_rate_per_minute.index, error_rate_per_minute.values, color="tab:red")
    ax.set_title("Error Rate per Minute")
    ax.set_xlabel("Time")
    ax.set_ylabel("Error Rate (%)")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "error_rate.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.3 latency_distribution.png (叠加整体 P50/P95/P99)
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.hist(df["latency"], bins=40, edgecolor="black", alpha=0.8)
    ax.axvline(overall_p50, color="tab:green", linestyle="--", label=f"P50={overall_p50:.0f}ms")
    ax.axvline(overall_p95, color="tab:orange", linestyle="--", label=f"P95={overall_p95:.0f}ms")
    ax.axvline(overall_p99, color="tab:red", linestyle="--", label=f"P99={overall_p99:.0f}ms")
    ax.set_title("Latency Distribution")
    ax.set_xlabel("Latency (ms)")
    ax.set_ylabel("Frequency")
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_dir / "latency_distribution.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.4 latency_percentiles.png（P50/P95/P99 随时间变化，用来定位异常时段）
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(p50_per_minute.index, p50_per_minute.values, label="P50")
    ax.plot(p95_per_minute.index, p95_per_minute.values, label="P95")
    ax.plot(p99_per_minute.index, p99_per_minute.values, label="P99")
    ax.set_title("Latency Percentiles over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Latency (ms)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "latency_percentiles.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # 4.5 service_comparison.png
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.boxplot(latency_by_service, tick_labels=services)
    ax.set_title("Latency by Service")
    ax.set_xlabel("Service")
    ax.set_ylabel("Latency (ms)")
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "service_comparison.png", dpi=150, bbox_inches="tight")
    plt.close(fig)

    # ------------------------- 5. Interpret -------------------------
    print("\n=== 分析结论 ===")
    print(f"总请求数: {len(df)}")
    print(f"整体 P50/P95/P99 latency: {overall_p50:.0f} / {overall_p95:.0f} / {overall_p99:.0f} ms")
    print(f"整体 error rate: {df['is_error'].mean() * 100:.2f}%")
    worst_service = max(services, key=lambda s: np.median(df.loc[df["service"] == s, "latency"]))
    print(f"延迟中位数最高的服务: {worst_service}")

    print(f"\n5 张图已保存到: {output_dir}")


if __name__ == "__main__":
    main()
