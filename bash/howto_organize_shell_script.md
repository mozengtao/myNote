# 大型 Bash 项目的组织方式——以「网站发布」为例

下面以一个比较典型的场景作为例子：

> 将一个 Web 应用发布到多台 Linux 服务器。

整个流程：

```
开发者
    │
    ▼
build
    │
    ▼
upload
    │
    ▼
stop service
    │
    ▼
deploy
    │
    ▼
start service
    │
    ▼
health check
```

这是一个典型的 Shell 编排项目。

---

# 一、不推荐的写法（Everything Everywhere）

很多 Shell 最开始都会写成这样：

```bash
deploy()
{
    tar czf app.tar.gz app/

    for host in web01 web02 web03
    do
        scp app.tar.gz "$host:/tmp"

        ssh "$host" "
            systemctl stop nginx

            rm -rf /opt/app

            mkdir -p /opt/app

            tar xzf /tmp/app.tar.gz -C /opt/app

            systemctl start nginx

            curl -fs http://127.0.0.1/health
        "
    done
}
```

问题：

```
deploy()

│

├──tar

├──scp

├──ssh

├──systemctl

├──mkdir

├──curl

├──各种字符串

└──各种细节
```

所有东西全部混在一起。

如果：

- ssh 参数变了
- 上传方式改了
- nginx 改成 apache

整个函数都会改。

这就是：

> 高耦合（High Coupling）

---

# 二、第一步：抽取 Primitive（命令原语）

例如：

```bash
archive_app()
{
    tar czf app.tar.gz app/
}

upload_file()
{
    local host=$1
    local src=$2
    local dst=$3

    scp "$src" "$host:$dst"
}

remote_exec()
{
    local host=$1
    shift

    ssh "$host" "$@"
}
```

然后：

```bash
stop_service()
{
    remote_exec "$1" systemctl stop nginx
}

start_service()
{
    remote_exec "$1" systemctl start nginx
}

deploy_archive()
{
    local host=$1

    remote_exec "$host" "
        rm -rf /opt/app &&
        mkdir -p /opt/app &&
        tar xzf /tmp/app.tar.gz -C /opt/app
    "
}

health_check()
{
    remote_exec "$1" curl -fs http://127.0.0.1/health
}
```

现在：

每个 Primitive：

只完成一个动作。

---

# 三、第二步：编排 Workflow

现在：

业务：

```bash
deploy_host()
{
    local host=$1

    upload_file "$host" app.tar.gz /tmp/

    stop_service "$host"

    deploy_archive "$host"

    start_service "$host"

    health_check "$host"
}
```

再进一步：

```bash
deploy_cluster()
{
    archive_app

    for host in "${HOSTS[@]}"
    do
        deploy_host "$host"
    done
}
```

整个流程：

```
deploy_cluster

↓

archive

↓

deploy_host

↓

upload

↓

stop

↓

deploy

↓

start

↓

health check
```

读起来：

像一本说明书。

---

# 四、数据再独立出来

不要：

```bash
deploy_host()
{
    upload_file web01 ...
}
```

而是：

config.sh

```bash
HOSTS=(
    web01
    web02
    web03
)

APP_DIR=/opt/app

ARCHIVE=app.tar.gz

SERVICE=nginx
```

Workflow：

```
不知道：

nginx

web01

web02

...

只知道：

HOSTS
SERVICE
```

数据：

完全独立。

---

# 五、推荐的目录组织

```
deploy/

├── main.sh
│
├── config/
│     config.sh
│
├── lib/
│
│    archive.sh
│    ssh.sh
│    upload.sh
│    service.sh
│    health.sh
│
├── workflow/
│
│    deploy.sh
│    rollback.sh
│
└── app/
```

其中：

```
config/
```

全部都是：

数据。

```
lib/
```

全部都是：

Primitive。

```
workflow/
```

全部都是：

编排。

---

# 六、各层对应关系

## Data Layer

```
HOSTS

SERVICE

APP_DIR

ARCHIVE
```

回答：

> 我有什么？

---

## Primitive Layer

```
archive_app()

upload_file()

remote_exec()

stop_service()

start_service()

health_check()
```

回答：

> 一个动作怎么完成？

---

## Workflow Layer

```
deploy_host()

deploy_cluster()

rollback_cluster()
```

回答：

> 为了完成业务，要依次调用哪些动作？

---

## Execution Layer

真正执行：

```
tar

scp

ssh

systemctl

curl
```

回答：

> Linux 如何完成？

---

# 七、整个调用关系

```
deploy_cluster()

        │

        ▼

 archive_app()

        │

        ▼

 deploy_host()

        │

        ├──────────────┐
        ▼              ▼

 upload_file()   stop_service()

        │              │

        ▼              ▼

 deploy_archive()  start_service()

        │              │

        └──────┬───────┘

               ▼

        health_check()
```

注意：

Workflow：

根本不知道：

```
scp

ssh

tar

curl
```

这些细节。

---

# 八、为什么这样组织？

例如：

以后：

```
scp

↓

rsync
```

修改：

```
upload_file()
```

结束。

Workflow：

完全不用改。

例如：

以后：

```
systemctl

↓

supervisorctl
```

修改：

```
stop_service()

start_service()
```

Workflow：

一行不用改。

这就是：

> 抽象（Abstraction）

---

# 九、对应的软件架构

其实：

Shell：

```
deploy_cluster()

↓

deploy_host()

↓

upload_file()

↓

scp
```

对应：

Java：

```
Controller

↓

Service

↓

Repository

↓

Database
```

对应：

Go：

```
cmd/

↓

service/

↓

infra/

↓

os/exec
```

完全一致。

---

# 十、最终心智模型

```
                    用户目标
            "发布整个网站"

                     │
                     ▼

        ┌─────────────────────────┐
        │     Workflow Layer      │
        │ deploy_cluster()        │
        │ deploy_host()           │
        └────────────┬────────────┘
                     │ 调用原语
                     ▼
        ┌─────────────────────────┐
        │    Primitive Layer      │
        │ upload_file()           │
        │ stop_service()          │
        │ deploy_archive()        │
        │ start_service()         │
        │ health_check()          │
        └────────────┬────────────┘
                     │ 封装 CLI
                     ▼
        ┌─────────────────────────┐
        │    Execution Layer      │
        │ scp                     │
        │ ssh                     │
        │ tar                     │
        │ systemctl               │
        │ curl                    │
        └────────────┬────────────┘
                     │
                     ▼
                 Linux 系统
```

---

# 十一、可以套用到任何 Bash 项目的简化模板

```
project/

├── main.sh                 # 程序入口
│
├── config.sh               # 所有配置（Data）
│
├── lib/
│   ├── log.sh              # 日志原语
│   ├── file.sh             # 文件原语
│   ├── remote.sh           # SSH/SCP 原语
│   ├── service.sh          # 服务管理原语
│   └── util.sh             # retry、wait、check 等通用原语
│
├── workflow/
│   ├── deploy.sh           # 发布流程
│   ├── rollback.sh         # 回滚流程
│   └── upgrade.sh          # 升级流程
│
└── app/                    # 待发布内容
```

整个项目遵循一条简单而稳定的调用链：

```
Data
  │
  ▼
Workflow（决定做什么）
  │
  ▼
Primitive（决定怎么完成一个动作）
  │
  ▼
Linux CLI（真正执行）
  │
  ▼
Linux Kernel
```

一句话总结：

> **Data 描述资源，Workflow 描述业务流程，Primitive 封装系统能力，CLI 负责真正执行。高层只负责"编排"，低层只负责"执行"，每层只承担一种职责。**