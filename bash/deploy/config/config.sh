#!/usr/bin/env bash
#
# Data 层：只描述"我有什么资源"，不包含任何执行逻辑。
# Workflow 只知道 HOSTS / SERVICE / APP_DIR / ARCHIVE 这些名字，
# 不知道具体是 web01 还是 nginx。

# 目标主机列表
HOSTS=(
    web01
    web02
    web03
)

# 远端应用安装目录
APP_DIR=/opt/app

# 打包后的归档文件名
ARCHIVE=app.tar.gz

# 待管理的服务名
SERVICE=nginx

# 本地待发布内容目录（相对 deploy/ 根）
APP_SRC=app

# 健康检查地址
HEALTH_URL=http://127.0.0.1/health
