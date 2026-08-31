#!/usr/bin/env bash
# 演示: 一个进程内 fork 出父子，通过 eventfd(EFD_SEMAPHORE) 唤醒消费者
set -euo pipefail

cd "$(dirname "$0")"

ITEM_COUNT=20000

rm -f /dev/shm/shm_examples_eventfd

./eventfd_demo "$ITEM_COUNT"
