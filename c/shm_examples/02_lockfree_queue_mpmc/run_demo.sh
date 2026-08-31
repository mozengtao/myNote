#!/usr/bin/env bash
# 演示: 4 个生产者进程 + 2 个消费者进程，共享一个 MPMC 无锁队列
set -euo pipefail

cd "$(dirname "$0")"

NUM_PRODUCERS=4
NUM_CONSUMERS=2
COUNT_PER_PRODUCER=50000

rm -f /dev/shm/shm_examples_mpmc_queue

./init "$NUM_PRODUCERS" "$NUM_CONSUMERS"

pids=()

for ((i = 0; i < NUM_CONSUMERS; i++)); do
	./consumer "$i" &
	pids+=("$!")
done

for ((i = 0; i < NUM_PRODUCERS; i++)); do
	./producer "$i" "$COUNT_PER_PRODUCER" &
	pids+=("$!")
done

status=0
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

./verify "$NUM_PRODUCERS" "$COUNT_PER_PRODUCER" || status=1

exit $status
