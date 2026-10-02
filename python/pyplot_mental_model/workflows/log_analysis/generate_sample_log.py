#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成模拟的 Web 服务访问日志：data/application.log

日志格式（每行一条请求记录）：
    2026-09-28T10:00:01 service=api latency=120 status=200

用途：给 analyze_log.py 提供一份可复现的输入数据，
     模拟真实工程场景中"从日志文件解析出结构化数据"这一步。
"""

from pathlib import Path

import numpy as np
import pandas as pd

# ------------------------- 参数 -------------------------
np.random.seed(11)

SERVICES = ["api", "auth", "order", "payment", "search"]
SERVICE_WEIGHTS = [0.35, 0.2, 0.2, 0.1, 0.15]

# 不同服务的延迟基线（毫秒），模拟"payment 服务天生更慢"
LATENCY_BASE = {
    "api": 50,
    "auth": 40,
    "order": 90,
    "payment": 160,
    "search": 70,
}

N_REQUESTS = 20000  # 2 小时内的总请求数
START_TIME = pd.Timestamp("2026-09-28T10:00:00")
DURATION_SECONDS = 2 * 3600  # 2 小时


def generate_log_lines(n: int) -> list[str]:
    """生成 n 条符合日志格式的行。"""
    offsets = np.sort(np.random.uniform(0, DURATION_SECONDS, size=n))
    services = np.random.choice(SERVICES, size=n, p=SERVICE_WEIGHTS)

    lines = []
    for offset, service in zip(offsets, services):
        ts = START_TIME + pd.Timedelta(seconds=float(offset))

        latency = LATENCY_BASE[service] + np.random.gamma(shape=2.0, scale=12.0)
        # payment 服务在某个时间窗口内制造一次"延迟抖动"事件，方便后续分析发现异常
        if service == "payment" and 3000 < offset < 3600:
            latency += np.random.uniform(200, 400)

        # 状态码：绝大多数是 200，少量 4xx/5xx；payment 抖动窗口内错误率也升高
        if service == "payment" and 3000 < offset < 3600:
            status = np.random.choice([200, 500, 503], p=[0.7, 0.2, 0.1])
        else:
            status = np.random.choice(
                [200, 400, 404, 500, 503], p=[0.96, 0.015, 0.01, 0.01, 0.005]
            )

        lines.append(
            f"{ts.strftime('%Y-%m-%dT%H:%M:%S')} "
            f"service={service} "
            f"latency={latency:.0f} "
            f"status={status}"
        )
    return lines


def main() -> None:
    lines = generate_log_lines(N_REQUESTS)

    out_dir = Path(__file__).resolve().parent / "data"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "application.log"

    with out_path.open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"已生成 {len(lines)} 条日志: {out_path}")
    print("前 5 行示例：")
    for line in lines[:5]:
        print(f"  {line}")


if __name__ == "__main__":
    main()
