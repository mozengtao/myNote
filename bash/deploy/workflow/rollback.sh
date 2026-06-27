#!/usr/bin/env bash
#
# Workflow 层 - 回滚流程。
# 同样只编排 Primitive：停服务 -> 切回上一版本 -> 起服务 -> 健康检查。

# rollback_host HOST
rollback_host()
{
    local host=$1

    log WORKFLOW ">> rollback host: $host"

    stop_service "$host"
    # 真实场景里这里会切换符号链接到上一个版本目录
    remote_exec "$host" "ln -sfn $APP_DIR.prev $APP_DIR"
    start_service "$host"
    health_check "$host"
}

# rollback_cluster
rollback_cluster()
{
    log WORKFLOW "== rollback cluster start =="

    local host
    for host in "${HOSTS[@]}"
    do
        rollback_host "$host"
    done

    log WORKFLOW "== rollback cluster done =="
}
