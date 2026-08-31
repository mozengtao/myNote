#!/usr/bin/env bash
# 演示: 按检测到的 NUMA node 数量分配内存、fork 出绑定到对应节点的 worker 验证
set -euo pipefail

cd "$(dirname "$0")"

echo "--- 当前环境 NUMA 拓扑 ---"
if command -v numactl >/dev/null 2>&1; then
	numactl --hardware 2>/dev/null || true
else
	ls /sys/devices/system/node/ 2>/dev/null | grep -E '^node[0-9]+' || echo "(无法探测，可能未安装 numactl)"
fi
echo "--------------------------"

./numa_demo
