#!/usr/bin/env bash
#
# Workflow 层 - 只负责"编排"，读起来像一本说明书。
# 这里只调用 Primitive，完全不知道 scp / ssh / tar / curl 的存在。

# deploy_host HOST
# 单台主机的发布流程：上传 -> 停服务 -> 解包 -> 起服务 -> 健康检查。
deploy_host()
{
    local host=$1

    log WORKFLOW ">> deploy host: $host"

    upload_file "$host" "$ARCHIVE" "/tmp/$ARCHIVE"
    stop_service "$host"
    deploy_archive "$host"
    start_service "$host"
    health_check "$host"
}

# deploy_cluster
# 整个集群的发布：先打包一次，再逐台发布。
deploy_cluster()
{
    log WORKFLOW "== deploy cluster start =="

    archive_app

    local host
    for host in "${HOSTS[@]}"
    do
        deploy_host "$host"
    done

    log WORKFLOW "== deploy cluster done =="
}
