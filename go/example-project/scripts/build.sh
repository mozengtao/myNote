#!/usr/bin/env bash
# scripts/ 只放脚本，不放业务代码，这里是最简单的构建脚本示例。
set -euo pipefail

cd "$(dirname "$0")/.."

echo "==> go vet"
go vet ./...

echo "==> go build"
go build -o bin/server ./cmd/server

echo "==> built bin/server"
