#!/usr/bin/env bash
# 演示: mmap + madvise(MADV_HUGEPAGE) 的匿名共享内存，fork 验证跨进程可见性，
# 并检查 /proc/self/smaps 里的 AnonHugePages 确认大页是否生效
set -euo pipefail

cd "$(dirname "$0")"

echo "--- 当前环境 THP 配置 ---"
cat /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null || true
grep -E 'HugePages_Total|Hugepagesize' /proc/meminfo || true
echo "-------------------------"

./hugepage_demo
