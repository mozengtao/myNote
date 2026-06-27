#!/usr/bin/env bash
#
# Primitive 层 - 远程执行原语 + 日志辅助。
# 这是整个项目最底层的"封装系统能力"的地方。
# 真实项目里这里会真的调用 ssh；本 demo 用 echo 代替，无任何副作用。

# log LEVEL MESSAGE...
# 统一的分层日志输出，便于直观看到调用层次。
log()
{
    local level=$1
    shift
    printf '[%-9s] %s\n' "$level" "$*"
}

# remote_exec HOST COMMAND...
# 在远端主机上执行命令的唯一入口。
# 上层（service/health/archive）都通过它来访问 ssh，
# 因此将来 ssh 换成别的方式，只需改这一个函数。
remote_exec()
{
    local host=$1
    shift

    # 真实：ssh "$host" "$@"
    echo "    [ssh] $host -> $*"
}
