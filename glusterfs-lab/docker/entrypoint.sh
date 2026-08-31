#!/bin/bash
# glusterd 默认以 systemd service 方式管理，但容器里没有 systemd (PID 1 问题)。
# 这里直接把 glusterd 以前台模式 (-N = no-daemon) 作为 PID 1 启动，
# 这样 `docker stop` 发的信号能直接送到 glusterd，容器生命周期 == glusterd 生命周期。
mkdir -p /var/log/glusterfs
exec /usr/sbin/glusterd -N --log-level INFO
