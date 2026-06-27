#!/usr/bin/env bash
#
# Primitive 层 - 文件上传原语。

# upload_file HOST SRC DST
# 把本地文件上传到远端。
# 真实：scp "$src" "$host:$dst"
# 将来想换成 rsync，只改这一个函数即可，Workflow 不用动。
upload_file()
{
    local host=$1
    local src=$2
    local dst=$3

    echo "    [scp] $src -> $host:$dst"
}
