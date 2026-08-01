#!/usr/bin/env bash
# 用 configs/dev.json 启动服务，第一个参数可以覆盖默认配置文件路径。
set -euo pipefail

cd "$(dirname "$0")/.."

CONFIG_PATH="${1:-configs/dev.json}"

go run ./cmd/server -config "$CONFIG_PATH"
