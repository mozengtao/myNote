#!/usr/bin/env bash
# 演示: 1 个 producer 后台运行，1 个 consumer 校验收到的数据完整性
set -euo pipefail

cd "$(dirname "$0")"

COUNT=200000

rm -f /dev/shm/shm_examples_ring_spsc 2>/dev/null || true

./producer "$COUNT" &
ppid=$!

./consumer

status=0
wait "$ppid" || status=1

exit $status
