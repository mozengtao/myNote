# Bash 中“构造命令”的方式总结

## 核心原则

在 Bash 中，一个命令本质上是：

```text
Command + Arguments
```

例如：

```bash
ls -l /tmp
```

实际上是：

```text
argv[0] = ls
argv[1] = -l
argv[2] = /tmp
```

工程化脚本的关键是：

```text
构造 argv
而不是拼接字符串
```

---

# 1. 直接执行（最简单）

适用于：

- 命令固定
- 参数固定

```bash
ls -l /tmp
```

等价于：

```text
argv[0] = ls
argv[1] = -l
argv[2] = /tmp
```

优点：

- 简单
- 可读性高

缺点：

- 无法动态调整参数

---

# 2. 使用变量

适用于：

- 少量动态参数

```bash
dir="/tmp"

ls -l "$dir"
```

推荐：

```bash
"$var"
```

不推荐：

```bash
$var
```

原因：

```bash
dir="my dir"

ls -l $dir
```

会变成：

```bash
ls -l my dir
```

发生 Word Splitting。

---

# 3. 函数封装（推荐）

适用于：

- 固定逻辑
- 重复使用

```bash
list_dir() {
    ls -l "$1"
}

list_dir /tmp
```

优点：

```text
逻辑复用
职责清晰
便于测试
```

工程脚本中最常见。

---

# 4. 数组构造命令（最佳实践）

适用于：

- 动态参数
- 动态选项
- SSH
- Docker
- Kubernetes
- Nomad

## 示例

```bash
job="evc"
task="worker"

cmd=(
    nomad
    alloc
    exec
    -job "$job"
    -task "$task"
)

"${cmd[@]}"
```

执行：

```bash
nomad alloc exec \
    -job evc \
    -task worker
```

---

## 动态追加参数

```bash
cmd=(curl)

[[ -n "$token" ]] && cmd+=(
    -H
    "Authorization: Bearer $token"
)

cmd+=("$url")

"${cmd[@]}"
```

结果：

```bash
curl \
    -H "Authorization: Bearer xxx" \
    https://example.com
```

---

## 数组的优势

避免：

```text
Word Splitting
Glob Expansion
Quote Hell
eval 风险
```

这是生产环境最推荐的方式。

---

# 5. Pipeline 构造

适用于：

- 命令流
- CLI 自动化

## 推荐

```bash
{
    printf '%s\n' "unhide debug"
    printf '%s\n' "$cmd"
} | ncs_cli
```

---

## 不推荐

```bash
echo "unhide debug\n$cmd"
```

原因：

```text
echo 行为不一致
\n 是否展开依赖实现
```

---

# 6. Here Document

适用于：

- 多行输入
- SSH 自动化
- SQL 执行

```bash
cat <<EOF
show version
show running-config
EOF
```

---

## SSH 场景

```bash
ssh host bash -s <<'EOF'
hostname
uptime
EOF
```

优点：

```text
结构清晰
无需复杂转义
```

---

# 7. 从文件读取命令

适用于：

- 批量执行

commands.txt

```text
show version
show running-config
show alarms
```

脚本：

```bash
while IFS= read -r cmd; do
    run_command "$cmd"
done < commands.txt
```

---

# 8. 命令替换

适用于：

- 获取命令输出

## 推荐

```bash
hostname="$(hostname)"
```

---

## 不推荐

```bash
hostname=`hostname`
```

原因：

```text
可读性差
嵌套困难
```

---

# 9. 字符串拼接命令（谨慎）

## 示例

```bash
cmd="ls -l $dir"
```

执行：

```bash
bash -c "$cmd"
```

或者：

```bash
eval "$cmd"
```

问题：

```text
转义复杂
空格问题
注入风险
```

一般不推荐。

---

# 10. eval（最后手段）

## 示例

```bash
cmd="ls -l /tmp"

eval "$cmd"
```

风险：

```bash
dir="tmp; rm -rf /"

eval "ls $dir"
```

可能执行：

```bash
ls tmp
rm -rf /
```

因此：

```text
非必要不用 eval
```

---

# 11. SSH 场景最佳实践

## 不推荐

```bash
ssh host "grep $pattern file"
```

容易产生：

```text
本地展开
远端展开
引用冲突
```

---

## 推荐

```bash
ssh host bash -s <<'EOF'
grep foo file
EOF
```

或者：

```bash
cat script.sh | ssh host bash
```

---

# 12. 调试命令

推荐统一封装：

```bash
debug_cmd() {
    printf '[DEBUG] ' >&2
    printf '%q ' "$@" >&2
    printf '\n' >&2
}
```

使用：

```bash
debug_cmd "${cmd[@]}"
```

输出：

```text
[DEBUG] nomad alloc exec -job evc -task worker
```

---

# 13. 工程化决策表

| 场景 | 推荐方式 |
|--------|----------|
| 固定命令 | 直接执行 |
| 逻辑复用 | 函数 |
| 动态参数 | 数组 |
| 多行输入 | Here Doc |
| 命令流 | Pipeline |
| 获取输出 | $(...) |
| 批量执行 | read loop |
| SSH 执行 | ssh + here-doc |
| 动态字符串 | 谨慎 |
| eval | 尽量避免 |

---

# 最佳实践总结

```text
固定逻辑：
    function

动态参数：
    array

多行输入：
    here-doc

命令流：
    pipeline

获取结果：
    $(...)

远程执行：
    ssh + here-doc

调试：
    printf '%q '

避免：
    eval
    未引用变量
    echo 构造复杂输入
```

## 推荐模式

```bash
run_ncs_command() {

    local cmd=(
        nomad
        alloc
        exec
        -job "$JOB"
        -task "$TASK"
        ncs_cli
        -u "$USER"
    )

    debug_cmd "${cmd[@]}"

    {
        printf '%s\n' "unhide debug"
        printf '%s\n' "$NCS_COMMAND"
    } | "${cmd[@]}"
}
```

这是 Bash 自动化脚本中最安全、最易维护、最具工程化风格的模式之一。