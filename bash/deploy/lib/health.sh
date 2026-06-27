#!/usr/bin/env bash
#
# Primitive 层 - 健康检查原语。

# health_check HOST
# 通过 remote_exec 调 curl 探活。
health_check()
{
    remote_exec "$1" "curl -fs $HEALTH_URL"
}
