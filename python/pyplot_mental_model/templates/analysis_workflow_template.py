#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
分析工作流模板（Analysis Workflow Template）

这不是一个"能直接跑出结果"的脚本，而是一个可复用的骨架——
拿到任何新的数据分析任务时，复制这个文件，把每个 TODO 替换成具体逻辑即可。

固定流程：
    Load -> Clean -> Explore -> Aggregate -> Compute Statistics
         -> Visualize -> Interpret -> Validate -> Export

对应关系：
    Load / Clean / Explore / Aggregate / Compute Statistics  —— Pandas / NumPy 负责
    Visualize                                                 —— Matplotlib 负责
    Interpret / Validate                                      —— 人负责，写在注释/print 里
    Export                                                     —— fig.savefig() / df.to_csv()
"""

from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def load_data(source_path: Path) -> pd.DataFrame:
    """
    1. Load —— 从原始数据源读取数据。

    TODO: 根据实际数据源替换成:
        - pd.read_csv(source_path)
        - pd.read_json(source_path)
        - 数据库查询 / API 请求后转成 DataFrame
        - 正则解析日志文件（参考 workflows/log_analysis/analyze_log.py）
    """
    # 示例：假设是 CSV
    df = pd.read_csv(source_path)
    return df


def clean_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    2. Clean —— 处理缺失值、异常类型、重复行等数据质量问题。

    TODO: 根据实际情况替换成:
        - df = df.dropna(subset=[...])
        - df["col"] = pd.to_numeric(df["col"], errors="coerce")
        - df = df.drop_duplicates()
        - df["timestamp"] = pd.to_datetime(df["timestamp"])
    """
    df = df.copy()
    # df = df.dropna()
    return df


def explore_data(df: pd.DataFrame) -> None:
    """
    3. Understand/Explore —— 先看清楚数据长什么样，再决定怎么分析。
    这一步永远优先于"开始画图"，对应 08_failure_and_debug.md 的 Debug Mental Model
    第一、二层（Data / Shape）。
    """
    print("=== shape ===")
    print(df.shape)
    print("=== dtypes ===")
    print(df.dtypes)
    print("=== head ===")
    print(df.head())
    print("=== describe ===")
    print(df.describe())


def compute_statistics(df: pd.DataFrame) -> dict:
    """
    4. Compute Statistics —— 所有统计计算都在这一步完成，Matplotlib 不参与。

    TODO: 根据实际分析目标替换成具体统计量，例如：
        stats["mean"] = df["value"].mean()
        stats["median"] = df["value"].median()
        stats["p95"] = np.percentile(df["value"], 95)
        stats["by_group"] = df.groupby("category")["value"].mean()
    """
    stats: dict = {}
    # stats["mean"] = df["value"].mean()
    return stats


def visualize(df: pd.DataFrame, stats: dict, output_dir: Path) -> None:
    """
    5. Visualize —— 参考 03_chart_selection.md 决定图形类型，
    统一用 fig, ax = plt.subplots() 的 OO 写法。

    TODO: 根据实际分析目标替换成具体绘图逻辑，例如：
        fig, ax = plt.subplots(figsize=(9, 5))
        ax.plot(df["x"], df["y"])
        ax.axvline(stats["p95"], linestyle="--", label="P95")
        ax.set_title("...")
        ax.set_xlabel("...")
        ax.set_ylabel("...")
        ax.legend()
        fig.tight_layout()
    """
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.set_title("TODO: replace with real chart")
    fig.tight_layout()

    output_dir.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_dir / "template_output.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


def interpret(stats: dict) -> None:
    """
    6. Interpret —— 用人类语言把统计量翻译成结论，写在 print/注释里，
    而不是只留在代码里。

    TODO: 根据实际统计量替换，例如：
        print(f"P95 latency = {stats['p95']:.1f}ms，超过 SLA 阈值 200ms")
    """
    print("=== 结论（TODO: 替换成真实解读） ===")
    print(stats)


def validate() -> None:
    """
    7. Validate —— 图形是否可能误导？（参考 07_misleading_charts.md）
    常见检查项：
        - bar chart 的 ylim 是不是从 0 开始？
        - histogram 的 bins 数量是否经过交叉验证？
        - 是否把 correlation 误当成了 causation？
        - 双 Y 轴是否有必要、是否可能造成视觉误导？

    TODO: 针对具体图形逐项检查，必要时在这里加断言或打印警告。
    """
    pass


def main() -> None:
    base_dir = Path(__file__).resolve().parent
    source_path = base_dir / "data.csv"  # TODO: 替换成真实数据源路径
    output_dir = base_dir.parent / "output" / "template"

    # 1. Load
    # df = load_data(source_path)

    # 2. Clean
    # df = clean_data(df)

    # 3. Explore
    # explore_data(df)

    # 4. Compute Statistics
    # stats = compute_statistics(df)

    # 5. Visualize
    # visualize(df, stats, output_dir)

    # 6. Interpret
    # interpret(stats)

    # 7. Validate
    # validate()

    # 8. Export（如果还需要导出清洗后的数据本身）
    # df.to_csv(output_dir / "cleaned_data.csv", index=False)

    print("这是一个模板文件，请复制并按 TODO 填充具体逻辑后再运行。")


if __name__ == "__main__":
    main()
