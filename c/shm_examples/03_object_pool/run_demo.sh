#!/usr/bin/env bash
# 演示: 8 个 worker 进程并发争抢一个容量为 256 的共享对象池
set -euo pipefail

cd "$(dirname "$0")"

NUM_WORKERS=8
ITER_PER_WORKER=20000

rm -f /dev/shm/shm_examples_object_pool

./init

pids=()
for ((i = 0; i < NUM_WORKERS; i++)); do
	./worker "$i" "$ITER_PER_WORKER" &
	pids+=("$!")
done

status=0
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

EXPECTED=$((NUM_WORKERS * ITER_PER_WORKER))
./verify "$EXPECTED" || status=1

exit $status
