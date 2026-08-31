#!/usr/bin/env bash
# 演示: 1 个 writer 高频发布快照，4 个 reader 并发认领/校验/持有/释放
set -euo pipefail

cd "$(dirname "$0")"

VERSION_COUNT=20000
NUM_READERS=4
MAX_POLLS=2000000

rm -f /dev/shm/shm_examples_snapshot

pids=()
for ((i = 0; i < NUM_READERS; i++)); do
	./reader "$i" "$VERSION_COUNT" "$MAX_POLLS" &
	pids+=("$!")
done

./writer "$VERSION_COUNT"

status=0
for pid in "${pids[@]}"; do
	wait "$pid" || status=1
done

rm -f /dev/shm/shm_examples_snapshot

exit $status
