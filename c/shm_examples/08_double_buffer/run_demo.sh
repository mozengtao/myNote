#!/usr/bin/env bash
# 演示: 1 个 producer 高频发布帧，1 个 consumer 独占读取/校验/释放
set -euo pipefail

cd "$(dirname "$0")"

FRAME_COUNT=20000
MAX_POLLS=2000000

rm -f /dev/shm/shm_examples_double_buffer

./consumer "$FRAME_COUNT" "$MAX_POLLS" &
cpid=$!

./producer "$FRAME_COUNT"

status=0
wait "$cpid" || status=1

rm -f /dev/shm/shm_examples_double_buffer

exit $status
