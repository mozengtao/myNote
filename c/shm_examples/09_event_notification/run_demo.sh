#!/usr/bin/env bash
# 依次运行三个事件通知子示例：eventfd / sem / futex
set -euo pipefail

cd "$(dirname "$0")"

status=0

echo "=== eventfd demo ==="
(cd eventfd && ./run_demo.sh) || status=1

echo "=== sem (named semaphore) demo ==="
(cd sem && ./run_demo.sh) || status=1

echo "=== futex demo ==="
(cd futex && ./run_demo.sh) || status=1

exit $status
