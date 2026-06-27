# 大型 Bash 项目最佳实践：配置加载、配置校验与命令抽象的心智模式

> 对于大型 Bash 项目，一个成熟的入口（main.sh）通常遵循如下流程：
>
> ```
> 启动
>   │
>   ▼
> 加载配置(source)
>   │
>   ▼
> 校验配置(: "${VAR:?}")
>   │
>   ▼
> 初始化运行环境
>   │
>   ▼
> 调用 Workflow
>   │
>   ▼
> Workflow 调用 Primitive
>   │
>   ▼
> Primitive 调用 Linux Command
> ```
>
> 可以把整个项目理解成一个"小型软件系统"，而不是一堆 Shell 命令。

---

# 一、source：加载配置（Load Configuration）

## 基本用法

```bash
source config.sh
```

或者

```bash
. config.sh
```

二者完全等价。

作用：

> **在当前 Shell 环境执行另一个 Shell 文件。**

例如：

config.sh

```bash
HOST=web01
PORT=8080
IMAGE=myapp:v1
```

main.sh

```bash
source config.sh

echo "$HOST"
echo "$PORT"
```

输出：

```
web01
8080
```

---

## 为什么使用 source？

不要：

```bash
HOST=...
PORT=...
TOKEN=...
```

散落在每个脚本中。

而应该：

```
config.sh
```

统一管理：

```
所有配置
```

形成：

```
Config File

↓

source

↓

Shell Variables

↓

Workflow
```

这样：

- 修改配置不用改代码
- 多环境（dev/test/prod）容易切换
- 数据与逻辑彻底分离

---

# 二、:（Colon）——空命令（No-op）

## 基本作用

```bash
:
```

表示：

```
什么都不做
```

返回值永远是：

```
0
```

等价于：

```bash
true
```

但它是 Shell 内建命令。

---

## 为什么经常写成

```bash
: "$VAR"
```

不是为了执行 `:`。

而是：

利用 Shell 的：

```
Parameter Expansion
```

例如：

```bash
: "${HOST:?HOST missing}"
```

执行过程：

```
Parameter Expansion

↓

HOST 是否存在？

↓

存在

↓

调用 :

↓

什么都不做
```

因此：

> `:` 的真正作用是**提供一个合法的命令上下文，从而触发参数展开**。

---

# 三、`${variable:?message}` —— 配置校验（Configuration Validation）

语法：

```bash
${VAR:?message}
```

作用：

> 如果变量不存在或为空，立即输出错误并退出 Shell。

例如：

```bash
: "${HOST:?HOST not configured}"
```

若：

```bash
HOST=web01
```

继续执行。

若：

```bash
unset HOST
```

输出：

```
bash: HOST: HOST not configured
```

脚本退出。

---

## 为什么大型 Bash 项目喜欢这种写法？

相比：

```bash
if [[ -z "$HOST" ]]; then
    echo "HOST missing"
    exit 1
fi
```

只需要：

```bash
: "${HOST:?HOST missing}"
```

更加：

- 简洁
- 可读
- 统一

---

## 批量配置检查

```bash
source config.sh

: "${HOST:?HOST missing}"
: "${PORT:?PORT missing}"
: "${TOKEN:?TOKEN missing}"
: "${IMAGE:?IMAGE missing}"
```

启动脚本第一件事：

> 检查配置是否完整。

---

# 四、Primitive：命令抽象（Command Abstraction）

## 为什么要抽象？

假设：

很多地方都写：

```bash
scp file.tar host:/tmp

ssh host "
    systemctl restart nginx
"
```

以后：

```
scp

↓

rsync
```

所有地方都要修改。

---

## 正确方式

抽成 Primitive：

```bash
upload_file()
{
    local host=$1
    local file=$2

    scp "$file" "$host:/tmp"
}
```

业务层：

```bash
upload_file web01 app.tar.gz
```

不知道：

```
scp
```

以后：

改成：

```bash
rsync ...
```

业务：

完全不用修改。

---

# 五、参数转换（Argument Transformation）

以：

```bash
nomad_job_run()
{
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

为例。

调用：

```bash
nomad_job_run \
    app.nomad \
    "image=demo:v1" \
    "replicas=3"
```

Workflow 看到的是：

```
Job
↓

变量列表
```

Primitive 内部：

负责转换：

```
image=demo:v1

↓

-var image=demo:v1
```

最终：

```
nomad job run \
    -var image=demo:v1 \
    -var replicas=3 \
    app.nomad
```

---

## 心智模型

Workflow：

```
我有什么？
```

Primitive：

```
CLI 需要什么格式？
```

Primitive：

负责：

```
Business Argument

↓

CLI Argument
```

即：

```
Business API

↓

Primitive

↓

Linux CLI
```

---

# 六、Primitive 不只是执行命令

Primitive 的职责：

```
业务参数

↓

参数检查

↓

参数转换

↓

默认值处理

↓

日志

↓

错误处理

↓

CLI 调用
```

例如：

```bash
copy_file()
{
    local src=$1
    local dst=$2

    [[ -f "$src" ]] || return 1

    cp "$src" "$dst"
}
```

调用者：

不知道：

```
cp
```

也不知道：

```
文件检查
```

---

# 七、Workflow 与 Primitive 分离

例如：

Workflow：

```bash
deploy()
{
    build_image

    upload_package

    restart_service

    verify
}
```

Primitive：

```bash
upload_package()
{
    scp ...
}

restart_service()
{
    ssh ...
}
```

Workflow：

回答：

> 做什么？

Primitive：

回答：

> 怎么做？

---

# 八、推荐的入口模板

```bash
#!/usr/bin/env bash

set -euo pipefail

source config.sh

: "${HOST:?HOST missing}"
: "${PORT:?PORT missing}"
: "${TOKEN:?TOKEN missing}"

main()
{
    deploy
}

main "$@"
```

其中：

```
main
```

只负责：

```
Workflow
```

不会：

直接写：

```
ssh

scp

curl

jq
```

---

# 九、推荐的目录组织

```
project/

├── main.sh                 # 入口
│
├── config.sh               # 配置（Data）
│
├── lib/
│   ├── ssh.sh              # Primitive
│   ├── file.sh
│   ├── docker.sh
│   ├── util.sh
│   └── log.sh
│
├── workflow/
│   ├── deploy.sh           # Workflow
│   └── backup.sh
│
└── templates/
```

职责：

```
config/

↓

提供数据

--------------------

workflow/

↓

组织流程

--------------------

lib/

↓

封装 Linux 命令
```

---

# 十、整体心智模型

```
                Configuration
            (config.sh / env)

                    │
            source config.sh
                    │
                    ▼
          Shell Variables（数据）
                    │
                    ▼
      : "${VAR:?message}"（配置校验）
                    │
                    ▼
          Workflow（业务编排）
         deploy / backup / upgrade
                    │
                    ▼
     Primitive（命令原语 / Command API）
 upload_file / ssh_exec / service_restart
                    │
      参数转换、默认值、错误处理、日志
                    │
                    ▼
        Linux CLI（scp / ssh / curl）
                    │
                    ▼
               Linux Kernel
```

---

# 十一、最终心智模式（建议牢记）

可以用一句话概括大型 Bash 项目的设计思想：

> **source 负责"把配置加载进内存"，`${VAR:?}` 负责"在程序启动时完成配置校验"，Primitive 负责"把业务参数转换成 CLI 参数并封装系统命令"，Workflow 负责"描述业务流程"，最终由 Linux 命令完成实际执行。**

整个项目遵循一条稳定的数据与控制流：

```
Config（数据）
      │
      ▼
source（加载）
      │
      ▼
Validation（校验）
      │
      ▼
Workflow（决定做什么）
      │
      ▼
Primitive（决定怎么做）
      │
      ▼
Linux CLI（真正执行）
      │
      ▼
Operating System
```

牢记四个关键词：

- **Load（加载）**：`source config.sh`
- **Validate（校验）**：`: "${VAR:?message}"`
- **Abstract（抽象）**：Primitive 封装命令细节
- **Orchestrate（编排）**：Workflow 描述业务流程