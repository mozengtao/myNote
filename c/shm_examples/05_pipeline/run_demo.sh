#!/usr/bin/env bash
# 演示: capture -> decode -> display 三级流水线，每一级之间是一个
# 独立的 SPSC Ring Buffer（零拷贝，各阶段完全解耦）。
set -euo pipefail

cd "$(dirname "$0")"

FRAME_COUNT=200000

rm -f /dev/shm/shm_examples_pipeline_stage1 /dev/shm/shm_examples_pipeline_stage2

./display &
display_pid=$!

./decode &
decode_pid=$!

./capture "$FRAME_COUNT" &
capture_pid=$!

status=0
wait "$capture_pid" || status=1
wait "$decode_pid" || status=1
wait "$display_pid" || status=1

exit $status
