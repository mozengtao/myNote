# 场景描述

假设文件：

```text
show interface ${IFACE}
show route vrf ${VRF}
ping ${IP}
```

或者：

```text
show interface eth0
show route
show version
```

你的目标是：

```text
1. 逐行读取
2. 对命令做变量替换
3. 执行命令
4. 获取执行结果
```

---

# 一、首先明确一个原则

很多人会这样写：

```bash
while IFS= read -r cmd
do
    eval "$cmd"
done < commands.txt
```

虽然能工作：

```text
非常危险
```

原因：

```text
eval

= Shell再次解析字符串
```

例如：

```text
show route; rm -rf /
```

经过：

```bash
eval "$cmd"
```

会被当成：

```bash
show route
rm -rf /
```

执行。

因此：

```text
除非文件内容完全可信
否则不要使用 eval
```

---

# 二、最佳实践思路

核心问题其实分成两类：

```text
A:
替换后执行固定程序

B:
替换后执行任意Shell命令
```

两种方案完全不同。

---

# 三、场景A：固定程序（推荐）

例如：

文件：

```text
show interface ${IFACE}
show route vrf ${VRF}
```

最终都是发送给：

```bash
ncs_cli
```

执行。

---

例如：

```bash
run_ncs_command "$command"
```

---

推荐：

```bash
while IFS= read -r line
do
    command="${line//\$\{IFACE\}/eth0}"
    command="${command//\$\{VRF\}/blue}"

    run_ncs_command "$command"

done < commands.txt
```

---

心智模型：

```text
File
 │
 ▼

Template String

 │
 ▼

Parameter Expansion

 │
 ▼

Real Command String

 │
 ▼

run_ncs_command()
```

---

# 四、推荐的模板机制

例如：

commands.txt

```text
show interface {{IFACE}}
show route vrf {{VRF}}
```

---

Shell：

```bash
iface="eth0"
vrf="blue"

while IFS= read -r line
do
    cmd=${line//\{\{IFACE\}\}/$iface}
    cmd=${cmd//\{\{VRF\}\}/$vrf}

    run_ncs_command "$cmd"

done < commands.txt
```

---

优点：

```text
无需 eval
无需 bash 解析
逻辑清晰
安全
```

---

# 五、场景B：执行真正的Shell命令

例如：

文件：

```text
ip link show dev ${IFACE}
ping -c 1 ${IP}
```

替换后：

```text
ip link show dev eth0
ping -c 1 8.8.8.8
```

确实需要执行。

---

# 六、推荐方案：envsubst

文件：

```text
ip link show dev ${IFACE}
ping -c 1 ${IP}
```

变量：

```bash
export IFACE=eth0
export IP=8.8.8.8
```

---

处理：

```bash
while IFS= read -r line
do
    cmd=$(printf '%s\n' "$line" | envsubst)

    echo "Running: $cmd"

done < commands.txt
```

---

输出：

```text
ip link show dev eth0
ping -c 1 8.8.8.8
```

---

# 七、执行时不要直接 eval

很多人：

```bash
cmd=$(envsubst < template)

eval "$cmd"
```

不推荐。

---

更好的方式：

## 拆成数组

例如：

```bash
cmd="ip link show dev eth0"
```

解析：

```bash
read -r -a argv <<< "$cmd"

"${argv[@]}"
```

---

形成：

```text
argv[0] = ip
argv[1] = link
argv[2] = show
argv[3] = dev
argv[4] = eth0
```

最终：

```bash
execve()
```

行为。

---

# 八、工程实践推荐

对于运维脚本：

```text
commands.txt
```

内容：

```text
show interface {{IFACE}}
show route vrf {{VRF}}
```

---

推荐：

```bash
render_command()
{
    local line="$1"

    line=${line//\{\{IFACE\}\}/$IFACE}
    line=${line//\{\{VRF\}\}/$VRF}

    printf '%s\n' "$line"
}
```

---

主循环：

```bash
while IFS= read -r line || [[ -n "$line" ]]
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue

    cmd=$(render_command "$line")

    run_ncs_command "$cmd"

done < commands.txt
```

---

# 九、如果必须执行Shell命令

推荐封装：

```bash
execute_command()
{
    local cmd="$1"

    printf 'Executing: %s\n' "$cmd" >&2

    bash -c "$cmd"
}
```

---

然后：

```bash
while IFS= read -r line
do
    cmd=$(render_command "$line")

    execute_command "$cmd"

done < commands.txt
```

---

相比：

```bash
eval "$cmd"
```

更好。

因为：

```text
eval
    当前Shell执行

bash -c
    子Shell执行
```

隔离性更好。

---

# 十、你的场景（网络自动化/NCS/CLI）推荐方案

结合你之前的 NCS CLI 场景：

commands.txt

```text
show interface {{IFACE}}
show route vrf {{VRF}}
show subscriber {{MAC}}
```

---

推荐架构：

```text
commands.txt

      │
      ▼

read line

      │
      ▼

render_command()

      │
      ▼

show interface xe0

      │
      ▼

run_ncs_command()

      │
      ▼

ncs_cli
```

---

示例：

```bash
while IFS= read -r line || [[ -n "$line" ]]
do
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^[[:space:]]*# ]] && continue

    cmd=$(render_command "$line")

    printf '[INFO] Executing: %s\n' "$cmd" >&2

    run_ncs_command "$cmd"

done < commands.txt
```

---

# 最终最佳实践

## 不推荐

```bash
while read line
do
    eval "$line"
done
```

---

## 推荐（模板命令）

```bash
while IFS= read -r line
do
    cmd=$(render_command "$line")

    run_ncs_command "$cmd"

done < commands.txt
```

---

## 推荐（Shell命令）

```bash
while IFS= read -r line
do
    cmd=$(envsubst <<< "$line")

    bash -c "$cmd"

done < commands.txt
```

---

## 工程化心智模型

```text
commands.txt

    │
    ▼

Template Command

    │
    ▼

render_command()

    │
    ▼

Concrete Command

    │
    ├──────────────► run_ncs_command()
    │
    └──────────────► bash -c

    ▼

Result
```

关键原则：

```text
读取文件：
    while IFS= read -r

变量替换：
    Parameter Expansion
    或 envsubst

执行：
    优先调用固定函数
    次选 bash -c
    最后才考虑 eval
```