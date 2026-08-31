#!/usr/bin/env bash
# 演示: 1 个 writer 高频发布配置更新，4 个 worker 并发轮询 + seqlock 校验
set -euo pipefail

cd "$(dirname "$0")"

UPDATE_COUNT=20000
NUM_WORKERS=4
MAX_POLLS=5000000

rm -f /dev/shm/shm_examples_shared_config

pids=()
for ((i = 0; i < NUM_WORKERS; i++)); do
	./worker "$i" "$UPDATE_COUNT" "$MAX_POLLS" &
	pids+=("$!")
done

./config_writer "$UPDATE_COUNT"

status=0
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

rm -f /dev/shm/shm_examples_shared_config

exit $status
