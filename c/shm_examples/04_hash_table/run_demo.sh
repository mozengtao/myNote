#!/usr/bin/env bash
# 演示: 多进程写入共享哈希表 -> 多进程并发查询 -> 删除 -> 最终一致性校验
set -euo pipefail

cd "$(dirname "$0")"

NUM_WRITERS=8
COUNT_PER_WRITER=1000
NUM_READERS=4
TOTAL=$((NUM_WRITERS * COUNT_PER_WRITER))

rm -f /dev/shm/shm_examples_hash_table

./init

echo "--- writers ---"
pids=()
for ((i = 0; i < NUM_WRITERS; i++)); do
	./writer "$i" "$COUNT_PER_WRITER" &
	pids+=("$!")
done
status=0
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

echo "--- concurrent readers (full range, overlapping) ---"
pids=()
for ((i = 0; i < NUM_READERS; i++)); do
	./reader "$i" 0 "$TOTAL" &
	pids+=("$!")
done
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

echo "--- deleter (removes key % 3 == 0) ---"
./deleter 0 "$TOTAL" || status=1

echo "--- final verify ---"
./verify 0 "$TOTAL" || status=1

exit $status
