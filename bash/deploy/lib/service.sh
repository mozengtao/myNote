#!/usr/bin/env bash
#
# Primitive 层 - 服务管理原语。
# 通过 remote_exec 调 systemctl；将来换 supervisorctl 只改这里。

# stop_service HOST
stop_service()
{
    remote_exec "$1" "systemctl stop $SERVICE"
}

# start_service HOST
start_service()
{
    remote_exec "$1" "systemctl start $SERVICE"
}
