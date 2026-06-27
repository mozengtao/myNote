
- shell 函数的优化
```bash
# 1
run_ncs_command() {
    local command=$1

    printf '%s\n' "$command" |
        nomad alloc exec \
            -task evc \
            -job "$EVC_JOB" \
            ncs_cli -u admin
}

# 2
run_ncs_command() {
    nomad alloc exec \
        -task evc \
        -job "$EVC_JOB" \
        ncs_cli -u admin
}

run_ncs_command <<'EOF'
unhide debug
show cable modem brief
exit
EOF

# 3
run_in_evc() {
    nomad alloc exec \
        -task evc \
        -job "$EVC_JOB" \
        "$@"
}

ncs_cli_exec()
{
    run_in_evc ncs_cli -u admin
}

printf '%s\n' \
    "unhide debug" \
    "show cable modem brief" |
ncs_cli_exec

# 4
ncs_cli_exec() {
    local line

    for line in "$@"; do
        printf '%s\n' "$line"
    done |
    run_in_evc ncs_cli -u admin
}

ncs_cli_exec \
    "unhide debug" \
    "show cable modem brief | t | nomore"

# 5
run_in_evc()
{
    nomad alloc exec \
        -task evc \
        -job "$EVC_JOB" \
        "$@"
}

ncs_cli_exec()
{
    run_in_evc ncs_cli -u admin
}

ncs_run()
{
    printf '%s\n' "$@" |
        ncs_cli_exec
}

ncs_run \
    "unhide debug" \
    "show cable modem brief | t | nomore"

ncs_cli_exec <<'EOF'
unhide debug
show cable modem brief | t | nomore
show version
exit
EOF

# 三层 Primitive 的思想
Workflow
──────────────────────────
ncs_run
show_modem_status

        │
        ▼

Domain Primitive
──────────────────────────
ncs_cli_exec

        │
        ▼

Infrastructure Primitive
──────────────────────────
run_in_evc

        │
        ▼

Linux CLI
──────────────────────────
nomad alloc exec ...

越往上越接近业务语义，越往下越接近系统命令。这样不仅复用性高，而且当执行环境（例如从 nomad 切换到其他容器运行时）发生变化时，只需要修改最底层 Primitive
```

- bash primitive 实现
```bash
nomad_job_run() {
    local hcl=$1          # ① 提取第一个固定参数
    shift                 # ② 把它从参数列表移走，剩下的都是 var

    local -a args=()      # ③ 声明局部数组，用于累积转换结果
    local var
    for var in "$@"; do   # ④ 遍历"剩余可变参数"
        args+=(-var "$var")   # ⑤ 一对一转换：key=val → -var key=val
    done

    (                     # ⑥ 子shell 隔离：cd 不污染调用者
        cd "$HCL_DIR"
        nomad job run "${args[@]}" "$hcl"   # ⑦ 数组安全展开
    )
}

start_kafka() {
    log "starting kafka container"
    nomad_job_run "$KAFKA_HCL" \
        "force=true" \
        "kafka_cluster_node_count=1" \
        "cert_mgt_enabled=false" \
        "kafka_service_name_prefix=$SERVICE_PREFIX" \
        "image_path=$KAFKA_IMAGE_PATH"
}

start_ksqldb() {
    log "starting ksqldb container"
    nomad_job_run "$KSQLDB_HCL" \
        "force=true" \
        "cert_mgt_enabled=false" \
        "kafka_service_name=$KAFKA_SERVICE_NAME" \
        "ksqldb_service_name=$KSQLDB_SERVICE_NAME" \
        "image_path=$KSQLDB_IMAGE_PATH"
}
```

- 读取文件
```bash
# 配置项（一行内容）
value=$(<file)

# 普通文本文件
while IFS= read -r line
do
    ...
done < file

# 表格型CLI输出
awk ...

#
while read -r name age city
do
    ...
done

# 文件列表 (处理特殊文件名)
find . -type f -print0 |
while IFS= read -r -d '' file
do
    echo "$file"
done

# 需要随机访问所有行
mapfile -t lines < file
```

- ssh远程执行命令
```bash
#
cat > collect_bond.sh <<'EOF'
#!/usr/bin/env bash

bond_name=${1:-bond_nsi_pf}

echo "===== $(hostname) ====="

grep "Slave Interface" \
    "/proc/net/bonding/${bond_name}"
EOF

#
ssh server1 bash -s -- bond_nsi_pf < collect_bond.sh

#
bash -s
      │
      ▼
从 stdin 读取脚本

--
      │
      ▼
后面是脚本参数

# 与 xargs 结合
xargs -P10 -I{} \
sh -c '
    ssh "$1" bash -s -- bond_ssi_pf < collect_bond.sh
' _ {} < hosts.txt


# enhancement
# collect_bond.sh
#!/usr/bin/env bash

set -euo pipefail

echo "===== $(hostname) ====="
echo

for bond_file in /proc/net/bonding/*
do
    [[ -f "$bond_file" ]] || continue

    bond_name=$(basename "$bond_file")

    echo "===== $bond_name ====="

    grep "Slave Interface" "$bond_file"

    echo
done
```

- more
```bash
# 读取并替换文件命令中的特定部分并执行
# 替换后执行固定程序
# commands.txt
show interface ${IFACE}
show route vrf ${VRF}

#
iface="eth0"
vrf="blue"

while IFS= read -r line
do
    cmd=${line//\{\{IFACE\}\}/$iface}
    cmd=${cmd//\{\{VRF\}\}/$vrf}

    run_ncs_command "$cmd"

done < commands.txt

#
export IFACE=eth0
export IP=8.8.8.8

while IFS= read -r line
do
    cmd=$(printf '%s\n' "$line" | envsubst)

    echo "Running: $cmd"

done < commands.txt

# 执行时不要直接 eval，而是 拆成数组
cmd="ip link show dev eth0"

read -r -a argv <<< "$cmd"

"${argv[@]}"

# 工程实践
# commands.txt
show interface {{IFACE}}
show route vrf {{VRF}}

render_command()
{
    local line="$1"

    line=${line//\{\{IFACE\}\}/$IFACE}
    line=${line//\{\{VRF\}\}/$VRF}

    printf '%s\n' "$line"
}

while IFS= read -r line || [[ -n "$line" ]]
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue

    cmd=$(render_command "$line")

    run_ncs_command "$cmd"

done < commands.txt


# 替换后执行任意Shell命令
execute_command()
{
    local cmd="$1"

    printf 'Executing: %s\n' "$cmd" >&2

    bash -c "$cmd"
}

while IFS= read -r line
do
    cmd=$(render_command "$line")

    execute_command "$cmd"

done < commands.txt

# 示例
while IFS= read -r line || [[ -n "$line" ]]
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^[[:space:]]*# ]] && continue

    cmd=$(render_command "$line")

    printf '[INFO] Executing: %s\n' "$cmd" >&2

    run_ncs_command "$cmd"

done < commands.txt

# 最佳实践
# 模板命令
while IFS= read -r line
do
    cmd=$(render_command "$line")

    run_ncs_command "$cmd"

done < commands.txt

# Shell命令
while IFS= read -r line
do
    cmd=$(envsubst <<< "$line")

    bash -c "$cmd"

done < commands.txt
```