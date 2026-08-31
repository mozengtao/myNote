#!/usr/bin/env bash
# 演示: 两个完全独立启动的进程，通过命名信号量 sem_open 同步
set -euo pipefail

cd "$(dirname "$0")"

ITEM_COUNT=20000

rm -f /dev/shm/shm_examples_sem_data
rm -f /dev/shm/sem.shm_examples_sem_ready

./reader "$ITEM_COUNT" &
rpid=$!

./writer "$ITEM_COUNT"

status=0
wait "$rpid" || status=1

rm -f /dev/shm/shm_examples_sem_data
rm -f "/dev/shm/sem.shm_examples_sem_ready"

exit $status
