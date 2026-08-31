#!/usr/bin/env bash
# 演示: 两个完全独立启动的进程，通过共享内存里的 futex word 同步
set -euo pipefail

cd "$(dirname "$0")"

ITEM_COUNT=20000

rm -f /dev/shm/shm_examples_futex

./reader "$ITEM_COUNT" &
rpid=$!

./writer "$ITEM_COUNT"

status=0
wait "$rpid" || status=1

rm -f /dev/shm/shm_examples_futex

exit $status
