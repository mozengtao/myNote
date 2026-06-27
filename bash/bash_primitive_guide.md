# Bash Primitive（命令原语）的设计哲学、最佳实践与心智模式

> **一句话定义**
>
> Primitive（命令原语）就是 **Shell 世界里的"库函数（Library Function）"**。
>
> 它向上提供**稳定、易理解的 API**，向下隐藏 **Linux CLI 的复杂细节**。

对于下面这个例子：

```bash
nomad_job_run() {
    local hcl=$1
    shift

    local -a args=()
    local var
    for var in "$@"; do
        args+=(-var "$var")
    done

    (
        cd "$HCL_DIR"
        nomad job run "${args[@]}" "$hcl"
    )
}
```

真正抽象的不是：

```
nomad job run
```

而是：

> **"运行一个 Job"这个能力（Capability）**

Workflow 不需要知道：

- `-var`
- `cd`
- `job run`
- 参数顺序
- HCL 所在目录

它只需要表达：

```
运行 Kafka

↓

运行 KSQLDB

↓

运行 Redis
```

---

# 一、Primitive 的定位

大型 Bash 项目建议遵循四层架构：

```
            Workflow
       (Business Logic)

              │

              ▼

        Primitive API
      (Capability Layer)

              │

              ▼

        Linux Commands
    (CLI Implementation)

              │

              ▼

        Linux Kernel
```

例如：

```
start_kafka()

↓

nomad_job_run()

↓

nomad job run

↓

Linux
```

Primitive 就是：

> Workflow 和 CLI 之间的适配层（Adapter）。

---

# 二、Primitive 最重要的职责

## 1）封装 CLI

Workflow：

```bash
start_kafka
```

不知道：

```
nomad job run
```

不知道：

```
-var
```

不知道：

```
cd
```

Primitive：

负责：

```
Business API

↓

CLI
```

---

## 2）参数转换（Argument Transformation）

例如：

Workflow：

```bash
nomad_job_run \
    kafka.hcl \
    "image=xxx" \
    "force=true"
```

Primitive：

转换：

```
image=xxx

↓

-var image=xxx
```

最终：

```
nomad job run \
    -var image=xxx \
    -var force=true \
    kafka.hcl
```

Workflow：

永远不用知道：

```
-var
```

---

## 3）默认行为

Primitive：

可以统一：

```bash
cd "$HCL_DIR"
```

Workflow：

无需：

```
cd
```

以后：

目录改变：

Primitive：

改一处。

---

## 4）统一错误处理

Primitive：

例如：

```bash
nomad_job_run()
{
    log "running job"

    nomad job run "$@"

    log "done"
}
```

Workflow：

不用：

```
echo

exit

...
```

全部：

统一。

---

## 5）统一日志

例如：

Primitive：

```bash
docker_exec()
{
    log "docker exec $*"

    docker exec "$@"
}
```

Workflow：

不用：

```
echo ...
```

---

## 6）统一 Retry

Primitive：

```bash
curl_retry()
{
    retry 5 curl "$@"
}
```

Workflow：

写：

```
curl_retry
```

即可。

---

# 三、Primitive 最重要的思想

不要抽象：

```
命令
```

要抽象：

```
能力
```

例如：

不要：

```bash
run_nomad_job()
```

而应该：

```
deploy_job()
```

或者：

```
start_service()
```

为什么？

因为：

以后：

```
Nomad

↓

Kubernetes
```

Workflow：

不用改。

Primitive：

里面：

```
kubectl apply
```

即可。

---

# 四、Primitive 是 Adapter（适配器）

例如：

Workflow：

```
image_path

↓

Primitive

↓

-var image_path=...
```

Workflow：

表达：

```
业务语言
```

Primitive：

表达：

```
CLI 语言
```

ASCII：

```
Workflow

image_path

service_name

replicas

        │

        ▼

 Primitive

        │

        ▼

-var image_path=...

-var service_name=...

-var replicas=...
```

---

# 五、Primitive 是 Translator（翻译器）

Workflow：

```
启动 Kafka
```

Primitive：

翻译：

```
nomad job run ...
```

Workflow：

```
复制文件
```

Primitive：

翻译：

```
scp

↓

或者

rsync
```

Workflow：

完全不知道。

---

# 六、Primitive 是 Policy（策略）

例如：

统一：

```
超时时间

Retry

日志

工作目录

Namespace

Token
```

例如：

```bash
kubectl_apply()
{
    kubectl \
        --namespace default \
        apply \
        "$@"
}
```

Workflow：

不用：

```
namespace
```

---

# 七、Primitive 是 Boundary（边界）

建议：

Workflow：

永远不要出现：

```
ssh

docker

kubectl

nomad

curl
```

这些：

全部：

放到 Primitive。

Workflow：

只出现：

```
deploy

backup

upload

verify

wait_ready
```

---

# 八、Primitive 的设计原则（推荐）

## 原则一：一个 Primitive，一个能力

推荐：

```bash
upload_file()

download_file()

start_service()

stop_service()
```

不要：

```bash
deploy_everything()
```

因为：

不可复用。

---

## 原则二：参数使用业务语言

推荐：

```bash
deploy_job \
    "$JOB_FILE" \
    "$IMAGE"
```

不要：

```bash
nomad job run \
    -var image="$IMAGE"
```

Workflow：

应该：

不知道：

```
-var
```

---

## 原则三：显式参数优于隐式全局变量

推荐：

```bash
upload_file "$host" "$src" "$dst"
```

优于：

```bash
HOST=...

upload_file
```

原因：

- 易测试
- 易阅读
- 易复用

全局变量仅适合真正的全局配置（如 `HCL_DIR`、`NOMAD_ADDR`）。

---

## 原则四：内部可以复杂，接口必须简单

例如：

内部：

```
参数转换

日志

Retry

cd

错误处理

cleanup

trap
```

Workflow：

看到：

```
deploy_job image
```

即可。

---

## 原则五：Primitive 不包含业务流程

例如：

不要：

```bash
deploy_cluster()
```

放进：

Primitive。

应该：

Primitive：

```
start_service

stop_service

upload_file
```

Workflow：

```
deploy_cluster

↓

upload

↓

stop

↓

start

↓

verify
```

---

# 九、常见 Primitive 分类

## 文件系统

```bash
copy_file()

move_file()

extract_archive()

ensure_dir()

remove_dir()
```

---

## SSH

```bash
remote_exec()

remote_copy()

remote_script()
```

---

## Docker

```bash
docker_exec()

docker_logs()

docker_restart()
```

---

## Kubernetes

```bash
kubectl_apply()

kubectl_wait()

kubectl_logs()
```

---

## HTTP

```bash
http_get()

http_post()

health_check()
```

---

## Service

```bash
start_service()

stop_service()

restart_service()

wait_service_ready()
```

---

## Utility

```bash
retry()

wait_until()

log_info()

log_error()

die()
```

---

# 十、Primitive 的调用关系

```
                 Workflow

        start_kafka()

        start_ksqldb()

        deploy_cluster()

               │

               ▼

        Primitive Library

      nomad_job_run()

      remote_exec()

      health_check()

      retry()

               │

               ▼

      Linux CLI Commands

      nomad

      ssh

      curl

      jq

               │

               ▼

          Linux Kernel
```

Workflow：

像：

```
一本操作手册
```

Primitive：

像：

```
一个 SDK
```

Linux CLI：

像：

```
系统 API
```

---

# 十一、终极心智模式

可以把一个优秀的大型 Bash 项目理解成下面这幅图：

```
                 Business Intent
          （我要启动 Kafka）

                       │

                       ▼

          Workflow（描述业务）

      start_kafka()

      start_ksqldb()

      deploy_cluster()

                       │

        调用"能力"，而不是命令

                       ▼

        Primitive（Command SDK）

      nomad_job_run()

      upload_file()

      remote_exec()

      restart_service()

                       │

      参数转换 / 默认值 / Retry
      日志 / 校验 / 工作目录
      CLI 适配 / 错误处理

                       ▼

          Linux CLI Commands

      nomad job run

      ssh

      scp

      docker exec

      curl

                       ▼

                 Linux System
```

---

# 十二、Primitive 的实现技巧（Bash 实现层 / How to Implement）

前面 一~十一 讲的是"**Primitive 应该是什么**"（设计哲学）。
这一节回到最初那个例子，讲"**Primitive 在 Bash 里怎么写**"（实现技巧）。

先回看锚点例子：

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
```

这短短一段，至少用到了 7 个可复用的实现技巧。下面把它们逐一展开，并补充其它常见技巧。

---

## 技巧 1：位置参数的提取与收集（`$1` / `shift` / `"$@"`）

"固定参数在前、可变参数在后"是 Primitive 最常见的签名形态。

```bash
# 模式：取走 N 个固定参数，剩余全部作为"可变尾参"
deploy_job() {
    local job_file=$1     # 固定参数 1
    local image=$2        # 固定参数 2
    shift 2               # 移走前 2 个，"$@" 现在只剩额外 var

    local -a extra=()
    local kv
    for kv in "$@"; do
        extra+=(-var "$kv")
    done
    nomad job run -var "image=$image" "${extra[@]}" "$job_file"
}
```

要点：

```
$1 $2 ...   单个位置参数
$@          所有位置参数（推荐始终写成 "$@"）
$#          参数个数
shift [N]   左移 N 个（默认 1），用于"消费"已处理的参数
```

> 经验法则：**凡是要把剩余参数整体传给下游命令，永远写 `"$@"`（带引号）。** 不带引号会按 `IFS` 二次拆词，空格/通配符会出问题。

---

## 技巧 2：参数校验与默认值（让接口"健壮"而不是"脆弱"）

Primitive 是边界，应该在入口就把错误参数挡住。

```bash
upload_file() {
    local src=${1:?upload_file: 缺少 src 参数}     # 必填：缺失即报错退出
    local dst=${2:?upload_file: 缺少 dst 参数}
    local mode=${3:-0644}                          # 可选：带默认值

    [[ -f $src ]] || die "源文件不存在: $src"       # 业务级前置校验
    (( $# <= 3 )) || die "参数过多"

    install -m "$mode" "$src" "$dst"
}
```

常用展开运算符：

```
${var:-default}   var 为空时用 default（不赋值）
${var:=default}   var 为空时用 default 并赋值给 var
${var:?message}   var 为空时打印 message 到 stderr 并退出
${var:+value}     var 非空时才取 value（常用于"有则加 flag"）
```

---

## 技巧 3：数组累积与一对一转换（业务参数 → CLI flag）

这是锚点例子最核心的技巧：用数组把"业务语言"翻译成"CLI 语言"。

```bash
# key=val 列表 → 一串 -var key=val
build_var_args() {
    local -a out=()
    local kv
    for kv in "$@"; do
        out+=(-var "$kv")
    done
    printf '%s\n' "${out[@]}"   # 仅示意；实战通常直接在原函数内展开
}
```

数组要点：

```
local -a arr=()      声明局部索引数组
arr+=(x)             追加一个元素
arr+=(a b)           一次追加多个元素
"${arr[@]}"          安全展开为"每个元素一个词"（保留空格）
"${arr[*]}"          展开为"用 IFS 连接的单个字符串"
${#arr[@]}           元素个数
```

> 为什么不用字符串拼接 `args="$args -var $var"`？
> 因为字符串拼接在遇到含空格的值（如 `desc=hello world`）时会被重新拆词，**数组是 Bash 里传递"参数列表"唯一安全的方式**。

---

## 技巧 4：`${var:+...}` 实现"可选 flag"（条件性追加参数）

很多 CLI flag 是"有值才加，无值不加"。

```bash
http_get() {
    local url=$1
    local token=${2:-}     # 可能为空

    local -a opts=(--fail --silent --show-error)
    # token 非空时才追加 Authorization 头
    [[ -n $token ]] && opts+=(-H "Authorization: Bearer $token")

    curl "${opts[@]}" "$url"
}
```

或用参数展开一行搞定：

```bash
local -a auth=()
auth=( ${token:+ -H "Authorization: Bearer $token"} )
```

---

## 技巧 5：关联数组做"命名选项"（key→value 配置）

当选项是"具名键值"而非"顺序列表"时，用关联数组更清晰。

```bash
restart_service() {
    local name=$1; shift

    declare -A opt=( [timeout]=30 [namespace]=default )   # 默认值
    while (( $# )); do                                    # 覆盖默认值
        opt[${1%%=*}]=${1#*=}                             # key=val → opt[key]=val
        shift
    done

    kubectl --namespace "${opt[namespace]}" \
        rollout restart "deployment/$name" \
        --timeout "${opt[timeout]}s"
}

# 调用：restart_service api timeout=60 namespace=prod
```

要点：

```
declare -A m         声明关联数组
m[key]=val           赋值
"${m[key]}"          取值
"${!m[@]}"           所有 key
${1%%=*}             去掉第一个 = 及其右边（取 key）
${1#*=}              去掉第一个 = 及其左边（取 value）
```

---

## 技巧 6：标准化的标志解析（`while/case` 与 `getopts`）

对外暴露 `--flag value` 风格时，用 `while + case`：

```bash
remote_exec() {
    local host="" cmd="" port=22

    while (( $# )); do
        case $1 in
            --host)  host=$2; shift 2 ;;
            --port)  port=$2; shift 2 ;;
            --cmd)   cmd=$2;  shift 2 ;;
            --)      shift; break ;;
            -*)      die "未知选项: $1" ;;
            *)       break ;;
        esac
    done

    ssh -p "$port" "$host" "$cmd"
}
```

短选项（`-h -p`）则用内建 `getopts`：

```bash
parse() {
    local OPTIND opt host port=22
    while getopts "h:p:" opt; do
        case $opt in
            h) host=$OPTARG ;;
            p) port=$OPTARG ;;
            *) die "用法错误" ;;
        esac
    done
    shift $((OPTIND - 1))   # 消费已解析选项，"$@" 剩下位置参数
}
```

> 选择：参数少且固定 → 位置参数；需要可读的具名选项 → `while/case`；POSIX 短选项 → `getopts`。

---

## 技巧 7：子shell 隔离副作用（`( cd ...; cmd )`）

锚点例子用 `( ... )` 把 `cd` 关在子shell 里，**调用者的当前目录不受影响**。

```bash
run_in_dir() {
    local dir=$1; shift
    (
        cd "$dir" || exit 1     # 子shell 里 cd 失败 → 子shell 退出 → 退出码传回父
        "$@"
    )
}
```

子shell vs `pushd/popd`：

```
( cd dir; cmd )      首选：天然隔离 cwd / 局部变量 / set 选项；
                     退出码会作为子shell 退出码正确传回
pushd dir; ...; popd 需要手动配对，异常路径容易漏 popd 导致目录错乱
```

子shell 还能隔离：环境变量改动、`set -e`/`IFS` 等 shell 选项、`trap` 设置——很适合"临时改环境跑一条命令"的场景。

---

## 技巧 8：返回值的两条通道（退出码 vs stdout）

Bash 函数有两种"返回"，不要混用：

```bash
# 通道 A：退出码（true/false 语义，0 成功，非 0 失败）
service_healthy() {
    curl -fs "http://$1/health" >/dev/null   # 最后一条命令的退出码即函数退出码
}
if service_healthy "$host"; then echo up; fi

# 通道 B：stdout（返回"数据"，由调用方用 $(...) 捕获）
current_image() {
    docker inspect -f '{{.Config.Image}}' "$1"
}
img=$(current_image mycontainer)
```

> 黄金规则：**返回数据走 stdout，日志/进度走 stderr。**
> 如果 `log` 误写到 stdout，会污染 `$(...)` 捕获到的结果。

返回复杂结果可用 nameref（`local -n`，Bash 4.3+）避免子shell：

```bash
list_running() {
    local -n _out=$1            # _out 是调用方变量的引用
    mapfile -t _out < <(docker ps --format '{{.Names}}')
}
declare -a names
list_running names             # 直接写回 names，无需 $(...) 子shell
```

---

## 技巧 9：统一日志（始终输出到 stderr）

```bash
log()  { printf '[%s] %s\n' "$(date +%T)" "$*" >&2; }
warn() { printf '\033[33m[WARN]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[31m[ERR ]\033[0m %s\n' "$*" >&2; exit 1; }

docker_exec() {
    log "docker exec $*"
    docker exec "$@"     # "$@" 原样透传，不二次拆词
}
```

---

## 技巧 10：统一错误处理（严格模式 + 退出码捕获 + cleanup）

脚本头部开启严格模式：

```bash
set -euo pipefail
#  -e  命令失败即退出
#  -u  使用未定义变量即报错
#  -o pipefail  管道中任一段失败，整条管道即失败
```

捕获下游退出码做分支处理：

```bash
kubectl_apply() {
    local rc=0
    kubectl --namespace default apply -f "$1" || rc=$?
    if (( rc != 0 )); then
        warn "apply 失败 (rc=$rc)，尝试回滚"
        kubectl rollout undo "deployment/$2"
        return "$rc"
    fi
}
```

用 `trap` 保证清理（无论成功/失败/中断都执行）：

```bash
with_tmpdir() {
    local tmp; tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' RETURN     # 函数返回时自动清理
    # ... 在 $tmp 里干活 ...
}
```

> 注意：`set -e` 在子shell `( ... )` 和命令替换里行为有差异；锚点例子里子shell 内若需要 `cd` 失败即停，要显式写 `cd "$dir" || exit 1`。

---

## 技巧 11：通用高阶 Primitive（用 `"$@"` 把命令当参数传）

把"命令本身"作为参数传入，可写出 `retry` / `wait_until` 这类可复用工具。

```bash
# 重试任意命令：retry <次数> <命令...>
retry() {
    local max=$1; shift
    local n=1
    until "$@"; do                       # 把剩余参数当作"一条命令"执行
        (( n++ >= max )) && { warn "重试 $max 次仍失败: $*"; return 1; }
        warn "第 $n 次重试: $*"
        sleep $(( n ))
    done
}

# 轮询直到条件成立：wait_until <超时秒> <命令...>
wait_until() {
    local timeout=$1; shift
    local deadline=$(( SECONDS + timeout ))
    until "$@"; do
        (( SECONDS >= deadline )) && return 1
        sleep 1
    done
}

# 组合使用
retry 5 curl -fs "$url"
wait_until 60 service_healthy "$host"
```

---

## 技巧 12：默认值与全局配置的安全初始化

真正的全局配置（如 `HCL_DIR`、`NOMAD_ADDR`）建议集中、带默认值、且只读。

```bash
: "${HCL_DIR:=/opt/nomad/jobs}"      # 外部未设置时给默认值
: "${NOMAD_ADDR:=http://127.0.0.1:4646}"
readonly HCL_DIR NOMAD_ADDR          # 固化，防止后续被意外改写
```

> 对应 原则三：**显式参数优于隐式全局变量**；全局变量只留给"全项目级别"的少数配置。

---

## 技巧 13：引号与展开安全（最容易踩的坑）

```
"$var"        防止空格/通配符二次拆词
"$@"          完整、逐个地透传所有参数（数组语义）
"${arr[@]}"   逐个展开数组元素
$( ... )      命令替换（优于反引号 ` `，可嵌套、可读）
printf '%q'   把值安全转义后再拼接（需要再次解析时）
```

反例与正解：

```bash
# 反例：未加引号，含空格的路径会被拆成两个参数
cp $src $dst

# 正解
cp -- "$src" "$dst"     # -- 还可防止文件名以 - 开头被当成选项
```

---

## 小结：把锚点例子拆成"可迁移技巧表"

```
锚点例子里的写法                  对应的通用技巧
------------------------------    --------------------------------
local hcl=$1; shift               技巧 1  固定参数提取 + 收集尾参
（隐含的入口校验位置）            技巧 2  参数校验与默认值
local -a args=(); args+=(...)     技巧 3  数组累积 + 一对一转换
（有则加 flag 的扩展）            技巧 4  ${var:+...} 可选 flag
（具名选项的扩展）                技巧 5  关联数组做命名选项
（--flag 风格的扩展）             技巧 6  while/case 或 getopts
( cd "$HCL_DIR"; ... )            技巧 7  子shell 隔离副作用
nomad job run 的返回              技巧 8  退出码 vs stdout 两条通道
（统一加日志）                    技巧 9  日志统一到 stderr
（统一错误处理/清理）            技巧 10 严格模式 + 退出码 + trap
（封装成 retry 等）              技巧 11 用 "$@" 写高阶 Primitive
$HCL_DIR 这类全局                技巧 12 默认值 + readonly
"${args[@]}" "$hcl"               技巧 13 引号与展开安全
```

> 一句话：**设计层决定"抽象成什么能力"，实现层（本节技巧）决定"这个能力在 Bash 里是否健壮、安全、可复用"。**

---

# 十三、更多经典实现技巧（进阶补充）

十二节覆盖了"从锚点例子直接拆出来"的 13 个基础技巧。
这一节补充工程实战中**同样高频、但更容易被忽略**的经典技巧（技巧 14 起继续编号）。

---

## 技巧 14：分离声明与赋值，保留退出码（最经典的坑）

`local x=$(cmd)` 会**吞掉 `cmd` 的退出码**——因为 `local` 自己几乎总是返回 0。
这是 Bash 里最常见的隐性 bug。

```bash
# 反例：即使 docker inspect 失败，rc 也是 0（local 的退出码覆盖了它）
current_image() {
    local img=$(docker inspect -f '{{.Config.Image}}' "$1")  # 退出码丢失
    echo "$img"
}

# 正解：先声明，再赋值，退出码得以保留
current_image() {
    local img
    img=$(docker inspect -f '{{.Config.Image}}' "$1") || return 1
    echo "$img"
}
```

> 规则：**凡是 `local`/`declare` 后面跟命令替换，一律拆成两行。**

---

## 技巧 15：依赖与前置断言（Fail Fast）

Primitive 作为边界，应在最早处确认"环境就绪"，而不是让下游命令抛出晦涩错误。

```bash
require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "缺少依赖命令: $1"
}

require_root() {
    (( EUID == 0 )) || die "需要 root 权限"
}

nomad_job_run() {
    require_cmd nomad        # 入口断言：没有 nomad 直接报错
    # ... 其余逻辑 ...
}
```

> `command -v` 是检查命令是否存在的可移植方式（优于 `which`，后者不一定存在且行为不一致）。

---

## 技巧 16：Dry-run（演练模式 / 可观测）

运维型 Primitive 应支持"只打印不执行"，便于审查与排错。

```bash
: "${DRY_RUN:=0}"

run() {                       # 统一执行入口
    if (( DRY_RUN )); then
        printf '+ %q ' "$@" >&2; printf '\n' >&2   # 仅打印，%q 安全转义
    else
        "$@"
    fi
}

docker_restart() {
    run docker restart "$1"   # DRY_RUN=1 时只回显命令
}

# 调用：DRY_RUN=1 docker_restart web
```

---

## 技巧 17：幂等性（Check-before-Act / ensure_*）

Primitive 重复执行应安全无副作用——这是可重试、可恢复的基础。

```bash
ensure_dir() {
    [[ -d $1 ]] || mkdir -p "$1"     # 已存在则什么都不做
}

ensure_line() {                       # 文件中不存在该行才追加
    local line=$1 file=$2
    grep -qxF -- "$line" "$file" 2>/dev/null || printf '%s\n' "$line" >> "$file"
}

ensure_service_running() {
    service_healthy "$1" || start_service "$1"
}
```

> 命名约定：幂等动作常用 `ensure_*` 前缀，向调用者传达"调用多少次结果都一样"。

---

## 技巧 18：Here-Doc / Here-String 喂 stdin（远程脚本经典写法）

把一段脚本通过 stdin 交给远端执行，避免转义地狱。

```bash
remote_script() {
    local host=$1
    # 'EOF' 加引号 → 本地不做变量展开，脚本原样送到远端
    ssh "$host" bash -s <<'EOF'
        set -euo pipefail
        df -h /
        systemctl is-active nginx
EOF
}

# Here-String：把单个字符串作为 stdin
jq_filter() { jq "$1" <<< "$2"; }
```

> 需要把**本地变量**注入远端时，去掉 `EOF` 的引号（`<<EOF`）即可启用展开，但要警惕注入风险。

---

## 技巧 19：安全读取命令输出到循环 / 数组

逐行处理命令输出时，`while IFS= read -r` + 进程替换是黄金组合（避免子shell 丢变量、避免拆词/转义问题）。

```bash
# 逐行遍历：用 < <(...) 而非 cmd | while（管道会让循环体在子shell 中，变量改不回来）
for_each_container() {
    local name
    while IFS= read -r name; do
        log "处理 $name"
    done < <(docker ps --format '{{.Names}}')
}

# 整块读入数组（Bash 4+）
list_containers() {
    local -n _out=$1
    mapfile -t _out < <(docker ps --format '{{.Names}}')
}
```

要点：

```
IFS=           关闭按空白拆分，保留行内空格
read -r        关闭反斜杠转义，原样读取
< <(cmd)       进程替换：循环体留在当前 shell，变量可保留
mapfile -t     读入数组并去掉行尾换行
```

---

## 技巧 20：用 `timeout` 防止卡死

任何可能挂起的下游（网络、远端、外部进程）都应设上限。

```bash
http_get() {
    timeout 10 curl -fsS "$1"          # 10 秒未完成即被终止
}

wait_service_ready() {
    timeout 60 bash -c '
        until curl -fs "http://'"$1"'/health" >/dev/null; do sleep 1; done
    '
}
```

> `timeout` 退出码为 124 表示超时，可据此分支处理。

---

## 技巧 21：用 `flock` 防止并发重入

部署/迁移类 Primitive 往往不能并发执行，用文件锁串行化。

```bash
with_lock() {
    local lockfile=$1; shift
    exec {fd}>"$lockfile"            # 打开锁文件，分配文件描述符
    flock -n "$fd" || die "已有实例在运行: $lockfile"
    "$@"                            # 持锁执行；fd 关闭（进程退出）即自动解锁
}

# 调用：with_lock /tmp/deploy.lock deploy_cluster
```

---

## 技巧 22：子命令分发表（Dispatch）

把一组相关能力收敛到一个入口，用 `case` 或关联数组分发——CLI 工具的经典骨架。

```bash
service_ctl() {
    local action=$1; shift
    case $action in
        start)   start_service   "$@" ;;
        stop)    stop_service    "$@" ;;
        restart) restart_service "$@" ;;
        status)  service_healthy "$@" ;;
        *)       die "未知动作: $action（支持 start|stop|restart|status）" ;;
    esac
}

# 调用：service_ctl restart web
```

---

## 技巧 23：TTY 检测与彩色降级

输出到终端时上色，输出到管道/文件时自动去色，避免污染日志。

```bash
if [[ -t 2 ]]; then                  # stderr 连到了终端？
    C_RED=$'\033[31m'; C_RST=$'\033[0m'
else
    C_RED=""; C_RST=""               # 非交互环境：禁用颜色
fi
warn() { printf '%s[WARN]%s %s\n' "$C_RED" "$C_RST" "$*" >&2; }
```

---

## 技巧 24：信号处理与全局清理（`trap EXIT/INT/TERM`）

脚本级别的资源清理应覆盖正常退出与中断信号。

```bash
declare -a _CLEANUP=()
cleanup() { local f; for f in "${_CLEANUP[@]}"; do eval "$f"; done; }
trap cleanup EXIT INT TERM           # 退出/Ctrl-C/被 kill 都执行

defer() { _CLEANUP+=("$1"); }        # 注册一条清理动作（类似 Go 的 defer）

with_tmp() {
    local tmp; tmp=$(mktemp -d)
    defer "rm -rf '$tmp'"            # 无论后续如何退出，都会清理
    # ... 使用 $tmp ...
}
```

> `INT`/`TERM` 之后通常还会触发 `EXIT`；只想清理一次时，可在 `cleanup` 里 `trap - EXIT` 防重入。

---

## 技巧 25：`printf '%q'` 安全拼装命令（日志 / dry-run / eval）

需要把"一条命令"序列化成字符串（打日志、回显、跨 shell 传递）时，用 `%q` 保证可被重新解析且无注入。

```bash
log_cmd() {
    local q; printf -v q '%q ' "$@"   # 把参数安全转义拼成一行
    log "exec: $q"
    "$@"
}

# desc=hello world 这样含空格的参数也能被正确转义、还原
log_cmd nomad job run -var "desc=hello world" kafka.hcl
```

---

## 技巧 26：结果缓存 / 记忆化（Memoization）

对"昂贵且短期内不变"的查询（如查 IP、查镜像 digest）做缓存，避免重复调用。

```bash
declare -A _CACHE                    # 进程内缓存
resolve_host() {
    local host=$1
    if [[ -z ${_CACHE[$host]+x} ]]; then          # +x 判断"键是否存在"
        _CACHE[$host]=$(getent hosts "$host" | awk '{print $1; exit}')
    fi
    printf '%s\n' "${_CACHE[$host]}"
}
```

> 这与缓存"带时效校验"的思路一致：可再加一个时间戳键，超过 TTL 则失效重取（类似常见的 `cmcache` 校验：先验时效，再验内容，命中才复用）。

---

## 技巧 27：受控并行（`&` + `wait`）

对相互独立的动作并发执行并收集结果，同时限制并发数。

```bash
parallel_each() {
    local max=$1; shift              # 最大并发
    local fn=$1; shift               # 对每个元素调用的函数名
    local item
    for item in "$@"; do
        "$fn" "$item" &              # 后台启动
        while (( $(jobs -rp | wc -l) >= max )); do wait -n; done   # 控并发
    done
    wait                             # 等全部完成
    # 注意：并行子进程的失败需各自落盘/上报，父进程逐个 wait <pid> 取退出码更精确
}

# 调用：parallel_each 4 restart_service web api worker cache
```

---

> 小结：十二节的技巧让一个 Primitive "**正确**"，本节技巧让它在真实运维环境里 **可观测、可重试、可并发、抗卡死、抗误用**——这正是"经典实现技巧"区别于"能跑就行"的地方。

---

# 十四、一句话总结（建议牢记）

**Primitive 不是对某个 Linux 命令的简单封装，而是对"一种能力（Capability）"的抽象。**

它承担的是 **Workflow 与 CLI 之间的适配器（Adapter）** 角色，负责：

1. **能力抽象（Capability Abstraction）**：接口表达业务能力，而不是 CLI 细节。
2. **参数翻译（Argument Translation）**：业务参数 → CLI 参数。
3. **策略封装（Policy Encapsulation）**：默认值、工作目录、Namespace、Token、Retry、日志等统一管理。
4. **复杂性隐藏（Complexity Hiding）**：Workflow 永远不知道底层命令如何组织。
5. **实现可替换（Replaceable Implementation）**：CLI、参数格式或平台发生变化时，只需修改 Primitive，不影响 Workflow。

因此，一个成熟的 Bash 项目遵循这样的调用链：

```
Business Intent
      │
      ▼
Workflow（What）
      │
      ▼
Primitive（How）
      │
      ▼
Linux CLI（Execute）
      │
      ▼
Operating System
```

> **Workflow 面向业务，Primitive 面向能力，CLI 面向系统。**
> **Workflow 决定"做什么"，Primitive 决定"如何完成一个动作"，CLI 负责真正执行。**