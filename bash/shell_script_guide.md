# Linux Shell Script 最佳实践（工程级指南）

> 目标：
>
> - 可维护（Maintainable）
> - 可读（Readable）
> - 可测试（Testable）
> - 可扩展（Scalable）
> - 可调试（Debuggable）
> - 可移植（Portable）

---

# 1. Shell 脚本的定位

Shell 的优势：

- 命令编排（Command Orchestration）
- 系统管理
- 自动化运维
- CI/CD
- 日志分析
- 文件处理

适合：

```text
Shell
 ├── 调用命令
 ├── 拼接命令
 ├── 管理流程
 ├── 管理文件
 └── 自动化任务

Shell Script
    │
    ├── 参数解析（getopts）
    ├── 日志模块（log_info/log_error）
    ├── 错误处理（trap ERR）
    ├── 资源清理（trap EXIT）
    ├── SSH模块
    ├── API模块（curl）
    ├── JSON模块（jq）
    ├── 文本处理模块（awk）
    ├── 配置模块（env.conf）
    └── 主流程(main)
```

不适合：

```text
× 复杂算法
× 高性能计算
× 大规模数据处理
× 复杂业务逻辑
```

超过 300~500 行：

```text
Shell -> Python
```

通常更合理。

---

# 2. 统一脚本模板

推荐所有脚本从如下模板开始

```bash
#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_NAME="$(basename "$0")"

main() {
    echo "hello"
}

main "$@"
```

---

# 3. set 最佳实践

## 推荐

```bash
set -Eeuo pipefail
```

含义：

### -e

命令失败立即退出

```bash
cp a b
rm file

echo done
```

如果 cp 失败：

```text
脚本立即退出
```

---

### -u

未定义变量报错

错误：

```bash
echo "$USERNAME"
```

如果没定义：

```text
unbound variable
```

避免：

```bash
echo ""
```

这种隐蔽 Bug。

---

### -o pipefail

默认：

```bash
cat not_exist | grep abc
```

返回：

```text
0
```

因为 grep 成功运行。

开启：

```bash
set -o pipefail
```

返回：

```text
非0
```

真正反映错误。

---

### -E

让 trap 能捕获函数错误。

---

# 4. 永远引用变量

错误：

```bash
rm -rf $dir
```

如果：

```bash
dir="my dir"
```

变成：

```bash
rm -rf my dir
```

两个参数。

正确：

```bash
rm -rf "$dir"
```

---

# 5. 优先使用 $( )

错误：

```bash
files=`ls`
```

正确：

```bash
files=$(ls)
```

支持嵌套：

```bash
result=$(cmd "$(other)")
```

---

# 6. 所有变量使用 local

坏例子：

```bash
foo() {
    count=10
}
```

污染全局。

正确：

```bash
foo() {
    local count=10
}
```

---

# 7. 常量使用 readonly

```bash
readonly API_URL="https://server/api"
```

或：

```bash
declare -r API_URL
```

---

# 8. 函数设计原则

推荐：

```bash
do_backup()
{
    local src="$1"
    local dst="$2"

    cp -r "$src" "$dst"
}
```

调用：

```bash
do_backup /data /backup
```

---

# 9. main() 入口模式

不要：

```bash
echo start

foo

bar
```

推荐：

```bash
main()
{
    foo
    bar
}

main "$@"
```

结构清晰。

---

# 10. 参数解析最佳实践

## 小脚本

```bash
file="$1"
```

---

## 工程脚本

使用 getopts

```bash
while getopts "f:o:v" opt
do
    case "$opt" in
        f) file="$OPTARG" ;;
        o) output="$OPTARG" ;;
        v) verbose=1 ;;
        *)
            exit 1
            ;;
    esac
done
```

调用：

```bash
script.sh -f input.txt -o result.txt
```

---

# 11. Usage 函数

```bash
usage()
{
    cat <<EOF
Usage:
    $SCRIPT_NAME -f FILE

Options:
    -f file
    -v verbose
EOF
}
```

---

# 12. 命令构造最佳实践

## 错误

```bash
cmd="grep $pattern $file"
$cmd
```

存在：

```text
word splitting
glob
注入
```

问题。

---

## 推荐数组

```bash
cmd=(
    grep
    "$pattern"
    "$file"
)

"${cmd[@]}"
```

---

# 13. SSH 场景

推荐：

```bash
ssh \
    -T \
    user@host \
    "hostname"
```

---

执行多条命令：

```bash
ssh user@host <<'EOF'
hostname
uptime
df -h
EOF
```

---

# 14. 管道最佳实践

错误：

```bash
cat file | grep abc
```

UUOC。

---

正确：

```bash
grep abc file
```

---

必要时：

```bash
journalctl -f |
grep ERROR
```

---

# 15. while read 最佳实践

错误：

```bash
cat file |
while read line
do
    ...
done
```

子 Shell 问题。

---

推荐：

```bash
while IFS= read -r line
do
    echo "$line"
done < file
```

---

# 16. 文件遍历最佳实践

错误：

```bash
for f in $(find .)
```

遇到空格炸掉。

---

正确：

```bash
find . -type f -print0 |
while IFS= read -r -d '' file
do
    echo "$file"
done
```

---

# 17. 日志函数

统一日志输出。

```bash
log_info()
{
    printf '[INFO ] %s\n' "$*" >&2
}

log_warn()
{
    printf '[WARN ] %s\n' "$*" >&2
}

log_error()
{
    printf '[ERROR] %s\n' "$*" >&2
}
```

---

# 18. Debug 模式

```bash
DEBUG=1
```

```bash
debug()
{
    [[ "${DEBUG:-0}" == 1 ]] || return

    printf '[DEBUG] %s\n' "$*" >&2
}
```

---

# 19. Trap 最佳实践

退出时清理资源。

```bash
tmpfile=$(mktemp)

cleanup()
{
    rm -f "$tmpfile"
}

trap cleanup EXIT
```

---

# 20. 错误处理框架

```bash
trap 'on_error $LINENO' ERR

on_error()
{
    local line="$1"

    echo "Failed at line $line"
}
```

---

# 21. mktemp

不要：

```bash
tmp=/tmp/test.txt
```

竞争风险。

推荐：

```bash
tmp=$(mktemp)
```

目录：

```bash
tmpdir=$(mktemp -d)
```

---

# 22. 检查依赖

```bash
require()
{
    command -v "$1" >/dev/null ||
    {
        echo "$1 not found"
        exit 1
    }
}
```

使用：

```bash
require jq
require curl
require ssh
```

---

# 23. JSON 处理

不要：

```bash
grep
sed
awk
```

解析 JSON。

推荐：

```bash
jq
```

```bash
jq -r '.name'
```

---

# 24. 获取脚本目录

```bash
SCRIPT_DIR=$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)
```

---

# 25. ShellCheck

必须使用：

:contentReference[oaicite:0]{index=0}

检查：

```bash
shellcheck script.sh
```

---

# 26. shfmt

统一格式。

```bash
shfmt -w script.sh
```

项目规范：

```bash
shfmt \
    -i 4 \
    -ci \
    -sr \
    -w .
```

---

# 27. Shell 测试框架

推荐：

:contentReference[oaicite:1]{index=1}

示例：

```bash
@test "hostname exists" {
    run hostname

    [ "$status" -eq 0 ]
}
```

---

# 28. Shell 项目目录结构

```text
project/

├── bin/
│   └── deploy.sh
│
├── lib/
│   ├── log.sh
│   ├── ssh.sh
│   └── utils.sh
│
├── tests/
│   └── deploy.bats
│
├── config/
│   └── env.conf
│
└── README.md
```

---

# 29. 大型脚本架构

```text
main
 │
 ├── parse_args
 │
 ├── validate
 │
 ├── load_config
 │
 ├── run
 │
 └── cleanup
```

代码：

```bash
main()
{
    parse_args "$@"

    validate

    load_config

    run

    cleanup
}
```

---

# 30. 工程级 Shell 心智模型

```text
                  Shell Script
                         │
                         ▼
               ┌────────────────┐
               │  Orchestrator  │
               └────────────────┘
                         │
      ┌──────────────────┼──────────────────┐
      ▼                  ▼                  ▼

   Command           File IO          Network IO
   Pipeline          Pipeline         Pipeline

      │                  │                  │
      ▼                  ▼                  ▼

 grep awk sed      read/write       ssh curl wget

      │                  │                  │
      └──────────────────┼──────────────────┘
                         ▼

                 Structured Output
                         │
                         ▼

                    Monitoring
                    Logging
                    Automation
```

---

# 31. 工程级 Shell 黄金法则

```text
1. 永远使用:
   set -Eeuo pipefail

2. 永远引用变量:
   "$var"

3. 所有函数变量:
   local

4. 所有常量:
   readonly

5. 命令构造:
   数组

6. 文件遍历:
   find -print0

7. 行读取:
   while IFS= read -r

8. 临时文件:
   mktemp

9. 错误处理:
   trap ERR

10. 退出清理:
    trap EXIT

11. JSON:
    jq

12. 代码检查:
    shellcheck

13. 代码格式化:
    shfmt

14. 超过500行:
    考虑迁移 Python

15. Shell负责编排(Command Orchestration)
    不负责复杂业务逻辑
```