# Shell 构造命令流（Generate Command Stream）最佳实践与心智模型

> 核心思想：
>
> Shell 中真正重要的不是"打印字符串"，而是**构造一个连续的数据流（Command Stream）**，供下游程序（ssh、bash、ncs_cli、mysql、psql、ftp 等）消费。

整个过程可以理解成 Linux Pipeline 的一种特殊应用：

```
                 Command Generator
                        │
                        ▼
                stdout (Command Stream)
                        │
          ┌─────────────┴─────────────┐
          ▼                           ▼
        pipe                        redirect
          │                           │
          ▼                           ▼
                 Command Consumer
```

例如：

```bash
printf 'show version\n' |
run_ncs host1
```

这里：

```
printf
   │
stdout
   │
 pipe
   │
stdin
   │
run_ncs
```

整个数据流如下：

```
Shell
 │
 ├── Generator
 │      │
 │      ▼
 │   Command Stream
 │
 └────────────► Consumer
```

---

# 一、printf（★★★★★ 最推荐）

## 场景

适合：

- 一条命令
- 少量命令
- 需要格式化变量
- POSIX Shell

例如：

```bash
printf 'show cable modem vmc %s brief | t\n' "$vmc" |
run_ncs "$host"
```

输出：

```
show cable modem vmc 100 brief | t
```

多个命令：

```bash
printf '%s\n' \
    "show version" \
    "show clock" \
    "show interface"
```

输出：

```
show version
show clock
show interface
```

### 优点

✔ POSIX

✔ 支持格式化

✔ 安全

✔ 推荐替代 echo

---

# 二、Here Document（★★★★★ 多行命令首选）

适合：

- 固定的大段 CLI
- SQL
- FTP
- SSH
- Router CLI

例如：

```bash
run_ncs "$host" <<EOF
show version
show clock
show cable modem vmc $vmc brief | t
EOF
```

数据流：

```
HereDoc
   │
stdin
   │
run_ncs
```

优点：

- 可读性最高
- 最接近人工输入

---

# 三、{ } Command Group（★★★★★）

适合：

- 多个 Generator
- 动态拼接

例如：

```bash
{
    printf 'show version\n'
    printf 'show clock\n'
    printf 'show cable modem vmc %s brief | t\n' "$vmc"
} |
run_ncs
```

流程：

```
printf
printf
printf
   │
   ▼
Command Group
   │
stdout
   │
pipe
   │
run_ncs
```

特点：

- 不创建 subshell
- 可以组合多个 Generator

---

# 四、cat + HereDoc（★★★★）

例如：

```bash
cat <<EOF |
show version
show clock
EOF
run_ncs
```

流程：

```
HereDoc
   │
 cat
   │
stdout
   │
pipe
   │
run_ncs
```

适合：

- 想明确表示"这里产生的是一段文本"

---

# 五、echo（★★）

例如：

```bash
echo "show version" |
run_ncs
```

适合：

- 调试
- 临时脚本

不推荐：

```
echo
```

存在：

- -n
- -e
- \c

不同 Shell 实现不同。

生产环境推荐：

```
printf
```

---

# 六、数组 + printf（★★★★★ 动态命令最佳实践）

例如：

```bash
commands=(
    "show version"
    "show clock"
    "show cable modem vmc $vmc brief | t"
)

printf '%s\n' "${commands[@]}" |
run_ncs
```

数据：

```
commands[]
        │
        ▼
printf
        │
stdout
        │
run_ncs
```

优点：

- 数据与逻辑分离
- 易维护
- 易 append

例如：

```bash
commands+=("show interface")
commands+=("show arp")
```

---

# 七、Generator Function（★★★★★）

例如：

```bash
generate_commands()
{
    printf 'show version\n'
    printf 'show clock\n'
    printf 'show cable modem vmc %s brief | t\n' "$vmc"
}

generate_commands |
run_ncs
```

结构：

```
generate_commands()
        │
stdout
        │
pipe
        │
run_ncs
```

这是大型 Shell 项目的经典组织方式。

---

# 八、循环生成（★★★★★）

例如：

```bash
for vmc in "${vmcs[@]}"
do
    printf 'show cable modem vmc %s brief | t\n' "$vmc"
done |
run_ncs
```

流程：

```
for
 │
 ├── printf
 ├── printf
 ├── printf
 │
 ▼
stdout
 │
 ▼
run_ncs
```

适合：

- 批量生成命令
- 数据驱动

---

# 九、模板文件（★★★★★）

commands.tpl

```
show version
show cable modem vmc ${VMC} brief | t
```

执行：

```bash
export VMC=100

envsubst < commands.tpl |
run_ncs
```

或者：

```bash
sed "s/@VMC@/$vmc/" commands.tpl |
run_ncs
```

适合：

- DevOps
- 配置模板
- 自动化部署

---

# 十、awk/sed/python Generator（★★★★）

例如：

```
vmc.list

100
101
102
```

awk：

```bash
awk '{print "show cable modem vmc "$1" brief | t"}' vmc.list |
run_ncs
```

Python：

```bash
python3 generate.py |
run_ncs
```

适合：

- 外部数据
- CSV
- JSON
- 数据库

---

# 十一、Process Substitution（★★★★）

例如：

```bash
run_ncs < <(
    printf 'show version\n'
)
```

流程：

```
Generator
    │
    ▼
 /dev/fd/63
    │
stdin
    │
run_ncs
```

适合：

Consumer 必须从 stdin 读取。

与 Pipe 最大区别：

```
Generator | Consumer
```

vs

```
Consumer < <(Generator)
```

---

# 十二、命令模板（★★★★★）

例如：

```bash
template='show cable modem vmc %s brief | t'

printf "$template\n" "$vmc" |
run_ncs
```

或者：

```bash
build_cmd()
{
    printf "$template\n" "$1"
}

build_cmd "$vmc" |
run_ncs
```

适合：

- CLI 自动化
- 多设备命令生成

---

# 十三、直接读取文件（★★★★★）

例如：

commands.txt

```
show version
show clock
show arp
```

执行：

```bash
run_ncs < commands.txt
```

流程：

```
commands.txt
      │
stdin
      │
run_ncs
```

适合：

- 固定命令
- 运维脚本
- 配置恢复

---

# 十四、动态组合 Generator（★★★★★）

例如：

```bash
{
    printf 'show version\n'

    if [[ $debug == yes ]]; then
        printf 'show logging\n'
    fi

    for vmc in "${vmcs[@]}"
    do
        printf 'show cable modem vmc %s brief | t\n' "$vmc"
    done
} |
run_ncs
```

这是实际工程中最常见的模式：

```
if
 │
for
 │
printf
 │
printf
 │
Command Group
 │
stdout
 │
run_ncs
```

---

# 各方法适用场景总结

| 方法 | 推荐指数 | 最适合场景 | 特点 |
|------|----------|-----------|------|
| `printf` | ⭐⭐⭐⭐⭐ | 单条/少量命令 | 安全、可格式化、POSIX |
| Here Document | ⭐⭐⭐⭐⭐ | 大段固定命令 | 可读性最佳 |
| `{ ... }` | ⭐⭐⭐⭐⭐ | 多 Generator 组合 | 灵活、不创建 subshell |
| `cat <<EOF` | ⭐⭐⭐⭐☆ | 文本模板 | 直观 |
| `echo` | ⭐⭐☆☆☆ | 调试 | 不建议生产使用 |
| 数组 + `printf` | ⭐⭐⭐⭐⭐ | 动态命令列表 | 数据与逻辑分离 |
| Generator Function | ⭐⭐⭐⭐⭐ | 大型脚本 | 模块化、易测试 |
| `for` 循环 | ⭐⭐⭐⭐⭐ | 批量命令 | 流式生成 |
| 模板文件 + `envsubst`/`sed` | ⭐⭐⭐⭐⭐ | 配置模板 | 配置与代码分离 |
| awk/sed/python | ⭐⭐⭐⭐☆ | 外部数据驱动 | 数据处理能力强 |
| Process Substitution | ⭐⭐⭐⭐☆ | Consumer 需读 stdin | 避免显式管道 |
| 命令模板变量 | ⭐⭐⭐⭐⭐ | CLI 自动化 | 易复用 |
| 输入重定向 `< file` | ⭐⭐⭐⭐⭐ | 固定命令文件 | 最简单 |
| 动态组合 Generator | ⭐⭐⭐⭐⭐ | 企业级复杂脚本 | 最具扩展性 |

---

# 推荐的统一心智模型（Generate Command Stream）

不要把 Shell 看成"执行命令"，而要看成"构造数据流"。

```
                    Data Source
                         │
      ┌──────────────────┼──────────────────┐
      │                  │                  │
    数组             HereDoc            文件/数据库
      │                  │                  │
      └──────────────────┼──────────────────┘
                         ▼
               Command Generator Layer
      (printf / for / awk / python / function)
                         │
                         ▼
              Command Stream (stdout)
                         │
      ┌──────────────────┼──────────────────┐
      │                  │                  │
      ▼                  ▼                  ▼
    Pipe               Redirect        Process Substitution
      │                  │                  │
      └──────────────────┼──────────────────┘
                         ▼
               Command Consumer Layer
     (run_ncs / ssh / bash / mysql / psql / ftp)
                         │
                         ▼
                    Remote System
                  CLI / Shell / Service
```

## 黄金原则

1. **把命令看成数据（Command as Data）**，而不是字符串拼接。
2. **Generator（命令生成）与 Consumer（命令执行）解耦**，通过标准输出/标准输入连接。
3. **数据与逻辑分离**：优先使用数组、模板文件或 Here Document 存放命令，逻辑负责选择和组合。
4. **优先选择流式处理（Streaming）**，避免不必要的临时文件，使生成、传输和消费可以连续进行。
5. **复杂脚本采用分层设计**：Data Source → Generator → Stream → Consumer，每一层职责单一、易于测试和复用。