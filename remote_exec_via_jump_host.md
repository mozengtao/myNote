# Remote Execution via Jump Host

> 生产级心智模型与实践指南

## 目录

- [1. 问题背景](#1-问题背景)
- [2. 架构模型](#2-架构模型)
- [3. 基础能力：ProxyJump](#3-基础能力proxyjump)
- [4. 核心问题：执行环境差异](#4-核心问题执行环境差异)
- [5. 解决方案](#5-解决方案)
- [6. 调试方法](#6-调试方法)
- [7. SSH 配置优化](#7-ssh-配置优化)
- [8. 自动化封装](#8-自动化封装)
- [9. 批量与并发执行](#9-批量与并发执行)
- [10. 核心原则](#10-核心原则)

---

## 1. 问题背景

### 典型的三段跳问题

```text
local → jump host (lxd) → target
```

### 传统手工流程的问题

```text
1. local → lxd（scp 脚本）
2. lxd → target（scp 脚本）  
3. ssh 登录 target 执行
4. target → lxd（scp 结果）
5. lxd → local（scp 结果）
```

### 问题根源

| 问题 | 描述 |
|------|------|
| ❌ 数据路径和控制路径耦合 | 控制流和数据流混在一起 |
| ❌ jump host 被当作文件中转站 | 误用为存储节点 |
| ❌ 操作冗余、不可扩展 | 手动步骤多，难以自动化 |

---

## 2. 架构模型

### 正确的架构设计

```text
✅ 推荐模型：
控制流：local → target（通过 jump host）
数据流：local ↔ target（同一 ssh 通道）
```

### Jump Host 的正确角色

| 角色 | 说明 |
|------|------|
| ✅ 连接能力扩展器（Proxy） | 提供网络连接能力 |
| ❌ 数据落盘职责 | 不应承担存储功能 |

---

## 3. 基础能力：ProxyJump

### 方案1：一次性执行（推荐）

```bash
ssh -J lxd -p 5617 tier3support@192.168.244.43 \
  'bash -lc "cat > /tmp/evccli.sh && chmod +x /tmp/evccli.sh && /tmp/evccli.sh \"show running-config vmc mac-domain | display json\""' \
  < evccli.sh \
  > vmc_mac_domain_cfg.json


# 用 ssh -J ... 'cmd' 执行的是 non-interactive / non-login shell
# 手动 ssh 执行的是 interactive login shell

scp -J lxd -P 5617 evccli.sh tier3support@192.168.244.43:/tmp/

ssh -J lxd -p 5617 tier3support@192.168.244.43 \
  'bash -lc "/tmp/evccli.sh \"show running-config vmc mac-domain | display json\""' \
| awk 'BEGIN{flag=0} /^\{/ {flag=1} flag {print}' \
| jq '.' \
> vmc_mac_domain_cfg.json

# 用 awk 做 "括号匹配"，过滤非 JSON 内容的噪音
ssh -J lxd -p 5617 tier3support@192.168.244.43 '
  bash -lc "
    cat > /tmp/evccli.sh &&
    chmod +x /tmp/evccli.sh &&
    /tmp/evccli.sh \"show running-config vmc mac-domain | display json\"
  "
' < evccli.sh \
| awk '
BEGIN {start=0; depth=0}
{
  if (!start && $0 ~ /^\{/) start=1
  if (start) {
    print
    depth += gsub(/\{/, "{")
    depth -= gsub(/\}/, "}")
    if (depth == 0) exit
  }
}
' \
| jq '.' \
> vmc_mac_domain_cfg.json

# 用 Python 做 "强解析"
ssh -J lxd -p 5617 tier3support@192.168.244.43 '
  bash -lc "
    cat > /tmp/evccli.sh &&
    chmod +x /tmp/evccli.sh &&
    /tmp/evccli.sh \"show running-config vmc mac-domain | display json\"
  "
' < evccli.sh \
| python3 -c '
import sys, json

data = sys.stdin.read()
start = data.find("{")

for i in range(start, len(data)):
    try:
        obj = json.loads(data[start:i])
        print(json.dumps(obj, indent=2))
        break
    except:
        pass
' \
> vmc_mac_domain_cfg.json

```

**数据流示意：**
```text
local(stdin) ─────────▶ target
local(stdout) ◀──────── target
```

### 方案2：分步执行（可调试）

```bash
# 1. 拷贝脚本
scp -J lxd -P 5617 evccli.sh tier3support@192.168.244.43:/tmp/

# 2. 执行（关键：login shell）
ssh -J lxd -p 5617 tier3support@192.168.244.43 \
  'bash -lc "/tmp/evccli.sh \"show running-config vmc mac-domain | display json\""'

# 3. 获取结果
scp -J lxd -P 5617 \
  tier3support@192.168.244.43:/tmp/out.json \
  ./vmc_mac_domain_cfg.json
```

---

## 4. 核心问题：执行环境差异

### 问题现象

| 执行方式 | 结果 |
|----------|------|
| 手动执行 | ✅ 正常 |
| 自动化执行 | ❌ 403/Permission denied |

### 根本原因

| Shell 类型 | 是否加载环境 | 说明 |
|------------|--------------|------|
| interactive login shell | ✅ | ssh 直接登录 |
| non-interactive shell | ❌ | ssh 'cmd' 方式 |

### 环境差异详解

**不会自动加载的文件：**
- `~/.bashrc`
- `~/.profile` 
- `/etc/profile`

**导致的问题：**
- PATH 丢失
- TOKEN/ACL 丢失  
- CLI context 丢失

---

## 5. 解决方案

### 方案1：强制 login shell（最推荐）

```bash
ssh -J lxd -p 5617 tier3support@192.168.244.43 \
  'bash -lc "/tmp/evccli.sh \"show running-config vmc mac-domain | display json\""'
```

**核心要点：**
```bash
bash -l -c "cmd"
```

### 方案2：手动加载环境

```bash
ssh -J lxd -p 5617 tier3support@192.168.244.43 '
  source /etc/profile
  source ~/.bashrc
  /tmp/evccli.sh "show running-config vmc mac-domain | display json"
'
```

### 方案3：脚本自带环境（生产级推荐）

```bash
#!/usr/bin/env bash
set -euo pipefail

# 初始化环境
source /etc/profile || true
source ~/.bashrc || true

CMD="$1"

echo "==== Executing: $CMD ===="
eval "$CMD"
```

---

## 6. 调试方法

### 环境对比调试

```bash
# 1. 获取交互式环境
env | sort > /tmp/env.interactive

# 2. 获取非交互式环境  
ssh -J lxd -p 5617 tier3support@192.168.244.43 \
  'env | sort' > /tmp/env.noninteractive

# 3. 对比差异
diff /tmp/env.interactive /tmp/env.noninteractive
```

---

## 7. SSH 配置优化

### 配置文件 `~/.ssh/config`

```config
Host lxd
  HostName <lxd-ip>

Host saratoga  
  HostName 192.168.244.43
  User tier3support
  Port 5617
  ProxyJump lxd

# 实现 "一键直达" 目标内网机器
# tail -n 9 ~/.ssh/config
# 最终目标机：coolserver
Host coolserver
    # 跳板机的真实 IP（或者是能解析的域名）, 继承你现有的 dhcpcool 配置（欺骗本地 SSH 第一步先连它）
    HostName 10.254.25.136
    # 跳板机的登录用户名
    User root
    # 登录到 dhcpcool 后，自动执行连接目标机的命令
    RemoteCommand ssh -tt -p 5617 -o StrictHostKeyChecking=no tier3support@10.254.25.203
    # 强制分配伪终端，确保交互式 Shell 正常工作
    RequestTTY yes

```

### 使用示例

```bash
# 直接连接
ssh saratoga

# 文件传输
scp evccli.sh saratoga:/tmp
```

---

## 8. 自动化封装

### 推荐脚本模板

```bash
#!/usr/bin/env bash
set -euo pipefail

JUMP_HOST="lxd"
TARGET="tier3support@192.168.244.43"
PORT=5617

SCRIPT=$1
CMD=$2
OUT=${3:-output.json}

ssh -J "$JUMP_HOST" -p "$PORT" "$TARGET" \
  "bash -lc \"cat > /tmp/runner.sh && chmod +x /tmp/runner.sh && /tmp/runner.sh '$CMD'\"" \
  < "$SCRIPT" \
  > "$OUT"

echo "[OK] result saved to $OUT"
```

---

## 9. 批量与并发执行

### 串行执行

```bash
for vmc in $(cat vmcnames.txt); do
  ./run_remote.sh evccli.sh \
    "show running-config vmc $vmc | display json" \
    "$vmc.json"
done
```

### 并发执行

```bash
cat vmcnames.txt | xargs -P 5 -I {} \
  ./run_remote.sh evccli.sh \
  "show running-config vmc {} | display json" \
  {}.json
```

---

## 10. 核心原则

### 错误 vs 正确的模型

| 模型 | 公式 | 说明 |
|------|------|------|
| ❌ 错误模型 | 执行 = 脚本 + 登录状态（隐式环境） | 依赖登录态 |
| ✅ 正确模型 | 执行 = 脚本 + 明确环境（显式） | 环境显式化 |

### 核心要点

> **不要把 jump host 当存储节点，而要把它当"连接能力扩展器"**
> 
> **不要依赖登录态环境，而要让执行环境显式化**

---

## 总结

通过跳板机进行远程执行的关键在于：

1. **正确理解 jump host 的角色定位** - 连接能力扩展器，而非存储节点
2. **处理好执行环境的差异问题** - 使用 login shell 或显式加载环境
3. **使用 ProxyJump 实现数据和控制流的统一** - 避免多步传输
4. **通过配置优化提升操作效率** - SSH 配置文件简化命令

### 最佳实践清单

- [ ] 使用 `ssh -J` 而不是多步 scp
- [ ] 强制使用 login shell (`bash -lc`)
- [ ] 配置 SSH config 简化命令
- [ ] 脚本自带环境初始化
- [ ] 使用并发执行提高效率
- [ ] 调试时对比环境差异

---

*文档版本：v1.0 | 更新时间：2026-05-13*