# 通用 Bash 模板：模板 × 变量集合 = 任务展开执行器

## 概述

这是一个基于"模板 × 变量集合 = 任务展开执行器"经典模式的通用bash模板，用于批量任务自动化执行。

## 核心架构

### 执行流程图

```
          +------------------+
          |  TEMPLATE FILE   |
          |  cmds.txt        |
          +------------------+
                    |
                    v
          +------------------+
          |   模板解析器      |
          |  (string replace)|
          +------------------+
                    |
                    v
+------------------+     +------------------+
|   VAR SET        |     | EXECUTOR         |
| vmcnames.txt     | --> | ./evccli.sh      |
+------------------+     +------------------+
                    |
                    v
            笛卡尔积展开
        (N vars × M templates)
```

### 三层抽象结构

| 层次 | 名称 | 职责 | 示例 |
|------|------|------|------|
| Layer 1 | 数据层 (Data) | 定义输入数据 | `VARS=(a b c)`, `CMDS=(cmd1 cmd2)` |
| Layer 2 | 展开层 (Expansion) | 模板变量替换 | `cmd=${template//VMC_NAME/$var}` |
| Layer 3 | 执行层 (Execution) | 实际任务执行 | `./evccli.sh "$cmd"` |

## 工业级模板代码

```bash
#!/usr/bin/env bash
set -euo pipefail

#######################################
# 配置区
#######################################
TEMPLATE_FILE="cmds.txt"
VAR_FILE="vars.txt"
DRY_RUN=${DRY_RUN:-0}        # 1=只打印不执行
PARALLEL=${PARALLEL:-0}      # >0 开启并发

#######################################
# 读取输入
#######################################
mapfile -t VARS < "$VAR_FILE"
mapfile -t CMDS < "$TEMPLATE_FILE"

#######################################
# 核心函数：单任务执行
#######################################
run_one() {
    local var="$1"
    local template="$2"

    # 模板替换（推荐：bash原生）
    local cmd=${template//VMC_NAME/$var}

    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[DRY] $cmd"
        return
    fi

    echo "[RUN] $cmd"
    ./evccli.sh "$cmd"
}

export -f run_one

#######################################
# 执行引擎
#######################################
for var in "${VARS[@]}"; do
    echo "===== $var ====="

    if [[ "$PARALLEL" -gt 0 ]]; then
        # 并发执行
        printf "%s\n" "${CMDS[@]}" \
            | xargs -I {} -P "$PARALLEL" bash -c 'run_one "$@"' _ "$var" {}
    else
        # 串行执行（最稳）
        for template in "${CMDS[@]}"; do
            run_one "$var" "$template"
        done
    fi
done
```

## 控制维度

| 维度 | 作用 | 配置方式 |
|------|------|----------|
| 串行 vs 并发 | 性能控制 | `PARALLEL=0` (串行) / `PARALLEL=5` (并发) |
| 干跑模式 | 安全预览 | `DRY_RUN=1` |
| 重试机制 | 稳定性保障 | 在执行函数中增加retry循环 |
| 日志记录 | 可观测性 | `echo "[INFO] $(date) $cmd"` |
| 限流控制 | 防止系统过载 | 控制并发数量 |

## 三个典型应用示例

### 示例1：批量服务器运维检查

**场景**: 对多台服务器执行健康检查命令

**变量文件 (servers.txt)**:
```
web-server-01
web-server-02
web-server-03
db-server-01
```

**模板文件 (health_check.txt)**:
```
ssh SERVER_NAME 'systemctl status nginx'
ssh SERVER_NAME 'df -h'
ssh SERVER_NAME 'free -m'
ssh SERVER_NAME 'uptime'
```

**执行**:
```bash
TEMPLATE_FILE="health_check.txt" VAR_FILE="servers.txt" ./batch_runner.sh
```

**结果**: 4台服务器 × 4条检查命令 = 16个任务自动执行

### 示例2：批量数据库操作

**场景**: 对多个数据库执行相同的SQL维护操作

**变量文件 (databases.txt)**:
```
user_db
order_db
payment_db
log_db
```

**模板文件 (maintenance.txt)**:
```
mysql -u admin -p DB_NAME -e "OPTIMIZE TABLE users;"
mysql -u admin -p DB_NAME -e "ANALYZE TABLE orders;"
mysql -u admin -p DB_NAME -e "SHOW TABLE STATUS FROM DB_NAME;"
```

**自定义替换版本**:
```bash
# 修改模板替换逻辑
local cmd=${template//DB_NAME/$var}
```

### 示例3：批量容器管理

**场景**: 对多个Docker容器执行管理操作

**变量文件 (containers.txt)**:
```
web-app-1
web-app-2  
api-gateway
redis-cache
```

**模板文件 (container_ops.txt)**:
```
docker stop CONTAINER_NAME
docker rm CONTAINER_NAME
docker pull myregistry/CONTAINER_NAME:latest
docker run -d --name CONTAINER_NAME myregistry/CONTAINER_NAME:latest
```

**执行器自定义**:
```bash
# 修改执行器为docker命令
echo "[RUN] $cmd"
eval "$cmd"  # 直接执行而不是调用外部脚本
```

### 示例
```bash
#!/usr/bin/env bash
set -euo pipefail

mapfile -t vmcs < vmcnames.txt
mapfile -t cmds < vmc_cmds.txt

for vmcname in "${vmcs[@]}"; do
    echo "--- Processing: $vmcname ---"

    for line in "${cmds[@]}"; do
        processed_cmd=${line//VMC_NAME/$vmcname}
        ./evccli.sh "$processed_cmd"
    done
done
```

```bash
# 通用参数化版本
#!/usr/bin/env bash
set -euo pipefail

#######################################
# 默认值
#######################################
VAR_FILE=""
TEMPLATE_FILE=""
EXEC_CMD=""
PLACEHOLDER="VMC_NAME"

DRY_RUN=0
PARALLEL=0

#######################################
# 帮助信息
#######################################
usage() {
    cat <<EOF
Usage: $0 -v <vars.txt> -t <templates.txt> -e <exec_cmd> [options]

Required:
  -v FILE     变量文件（每行一个变量）
  -t FILE     模板文件（包含占位符）
  -e CMD      执行命令（如: ./evccli.sh）

Optional:
  -p STR      占位符（默认: VMC_NAME）
  -n          dry-run（只打印不执行）
  -P NUM      并发数（默认: 串行）

Example:
  $0 -v vmcnames.txt -t vmc_cmds.txt -e ./evccli.sh
  $0 -v hosts.txt -t cmds.txt -e ssh -p HOST -P 5
EOF
    exit 1
}

#######################################
# 参数解析
#######################################
while getopts "v:t:e:p:nP:" opt; do
    case "$opt" in
        v) VAR_FILE="$OPTARG" ;;
        t) TEMPLATE_FILE="$OPTARG" ;;
        e) EXEC_CMD="$OPTARG" ;;
        p) PLACEHOLDER="$OPTARG" ;;
        n) DRY_RUN=1 ;;
        P) PARALLEL="$OPTARG" ;;
        *) usage ;;
    esac
done

[[ -z "$VAR_FILE" || -z "$TEMPLATE_FILE" || -z "$EXEC_CMD" ]] && usage

#######################################
# 读取数据
#######################################
mapfile -t VARS < "$VAR_FILE"
mapfile -t TEMPLATES < "$TEMPLATE_FILE"

#######################################
# 核心执行函数
#######################################
run_one() {
    local var="$1"
    local template="$2"

    local cmd=${template//${PLACEHOLDER}/$var}

    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "[DRY] $EXEC_CMD \"$cmd\""
        return
    fi

    echo "[RUN] $EXEC_CMD \"$cmd\""
    $EXEC_CMD "$cmd"
}

export -f run_one
export EXEC_CMD PLACEHOLDER DRY_RUN

#######################################
# 调度执行
#######################################
for var in "${VARS[@]}"; do
    echo "===== $var ====="

    if [[ "$PARALLEL" -gt 0 ]]; then
        printf "%s\n" "${TEMPLATES[@]}" \
            | xargs -I {} -P "$PARALLEL" bash -c 'run_one "$@"' _ "$var" {}
    else
        for template in "${TEMPLATES[@]}"; do
            run_one "$var" "$template"
        done
    fi
done

# 得到的能力
1.输入完全解耦
-v 任意变量文件
-t 任意模板文件
-e 任意执行器

2.占位符可变
-p HOST
-p DEVICE
-p USER
模板可以写成
ssh HOST "uptime"

3.执行器抽象
场景1：
./tool.sh -v vmcnames.txt -t vmc_cmds.txt -e ./evccli.sh
场景2：
./tool.sh -v hosts.txt -t cmds.txt -e ssh -p HOST
场景3：
./tool.sh -v ids.txt -t api.txt -e curl -p ID


支持复杂命令的安全写法
EXEC_CMD=(ssh user@host)
"${EXEC_CMD[@]}" "$cmd"
```

## 核心心智模式总结

### 1. 笛卡尔积执行器思维

**本质公式**: `任务总数 = 变量数量 × 模板数量`

- 30个VMC × 4条命令 = 120个任务
- 思维转换：从"写循环"到"定义空间"
- 优势：任务规模可预测，资源需求可计算

### 2. 数据-展开-执行分离原则

**分层思维**:
- **数据层**: 关注"输入是什么" - 纯数据，无逻辑
- **展开层**: 关注"如何生成任务" - 纯转换，无副作用  
- **执行层**: 关注"任务怎么跑" - 纯执行，无数据处理

**好处**: 各层职责清晰，易于调试和扩展

### 3. 控制维度思维

不是简单的"写脚本"，而是设计"执行策略":

| 维度 | 作用 | 心智模式 |
|------|------|----------|
| 并发控制 | 性能 vs 稳定性权衡 | 系统容量规划思维 |
| 干跑模式 | 安全 vs 效率权衡 | 渐进式部署思维 |
| 重试机制 | 可靠性保障 | 分布式系统容错思维 |
| 可观测性 | 运维友好 | DevOps运维思维 |

### 4. 系统抽象思维

这个模式在更大系统中的对应关系：

| Bash概念 | 系统设计对应 | 抽象层次 |
|----------|-------------|----------|
| VARS | Job Queue | 消息队列 |
| TEMPLATE | Job Template | 任务模板 |
| 展开 | Task Generation | 任务调度 |
| evccli.sh | Worker | 工作节点 |
| for loop | Scheduler | 调度器 |
| xargs -P | Concurrent Workers | 并发执行引擎 |

## 常见反模式与解决方案

### ❌ 反模式1：数据执行耦合
```bash
# 错误：边读边执行
for x in $(cat file); do
    command $x
done
```

### ✅ 正确模式：先读后执行
```bash
# 正确：数据读取与执行分离
mapfile -t VARS < "$VAR_FILE"
for var in "${VARS[@]}"; do
    command "$var"
done
```

### ❌ 反模式2：过度工具链
```bash
# 错误：简单替换用sed
cmd=$(echo "$template" | sed "s/VAR/$var/g")
```

### ✅ 正确模式：bash原生替换
```bash
# 正确：bash原生字符串替换
cmd=${template//VAR/$var}
```

## 进阶演进建议

### 增强可观测性
```bash
run_one() {
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') Starting: $cmd"
    # ... 执行逻辑
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') Completed: $cmd"
}
```

### 增加重试机制
```bash
run_one() {
    local max_retries=3
    for i in $(seq 1 $max_retries); do
        if ./evccli.sh "$cmd"; then
            break
        else
            echo "[WARN] Attempt $i failed, retrying..."
            sleep 2
        fi
    done
}
```

### 并发限流控制
```bash
# 不要一次性开启过多并发
PARALLEL=${PARALLEL:-5}  # 默认限制为5个并发
```

## 总结

这个模式的本质是构建一个**任务调度器 + 批处理执行引擎**。通过将"数据/展开/执行"三层分离，脚本从"能用"级别直接跃迁到"工程级"，具备了现代分布式系统的基本特征：可扩展、可观测、可控制。

这不仅仅是bash脚本技巧，更是一种系统性思维模式，适用于Ansible、Terraform、Kubernetes等现代基础设施工具的理解和使用。