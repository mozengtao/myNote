# Golang 项目组织最佳实践及心智模式

> **一句话理解：**
>
> **Go 项目 = Package（能力） + Module（版本） + Interface（契约） + Directory（边界）**
>
> Go 的目录不是为了"好看"，而是为了**表达依赖关系和系统边界（Boundaries）**。

---

# 一、Go 项目的整体心智模式

可以把一个 Go 项目想象成一家公司：

```
Company (Module)
│
├── HR
├── Finance
├── Sales
└── IT
```

对应 Go：

```
Module
│
├── package config
├── package database
├── package service
├── package api
└── package logger
```

每一个 Package 就像一个部门：

- 有自己的职责
- 对外暴露有限接口
- 内部细节别人不知道
- 尽量低耦合

所以：

> **Package 是 Go 项目的第一组织单位。**

不是目录。

不是 class。

不是 namespace。

而是 Package。

---

# 二、Go 项目的四层组织模型（推荐牢记）

```
Project
    │
    ▼
Module
    │
    ▼
Package
    │
    ▼
Type / Function
```

例如

```
myapp/
│
├── go.mod
│
├── cmd/
│      main.go
│
├── internal/
│      service/
│      storage/
│
├── pkg/
│      logger/
│
└── api/
```

对应关系：

```
Project
      │
      ▼
Module
      │
      ▼
Package(service)
      │
      ▼
UserService struct
```

---

# 三、目录组织真正解决什么问题？

不是为了分类。

而是为了：

> **管理依赖方向。**

例如：

错误：

```
service
     ↑
database
     ↑
handler
     ↑
service
```

循环依赖：

```
service
    ↔
database
```

Go 编译器直接报错。

所以：

好的目录组织就是：

```
handler
    │
    ▼
service
    │
    ▼
repository
    │
    ▼
database
```

依赖只有一个方向。

---

# 四、Go 官方推荐项目结构（最经典）

```
project/

├── cmd/
├── internal/
├── pkg/
├── api/
├── configs/
├── scripts/
├── web/
├── docs/
├── test/
├── go.mod
└── README.md
```

这是目前最广泛采用的结构。

---

# 五、每个目录应该放什么？

---

## ① cmd/

放：

> 可执行程序入口（main）

例如：

```
cmd/

    server/
        main.go

    cli/
        main.go

    worker/
        main.go
```

例如：

```
cmd/server/main.go

func main() {
    app.Run()
}
```

为什么？

因为：

一个 Module 可以有多个程序。

例如：

```
kubectl

kubelet

kube-apiserver

scheduler
```

都是 Kubernetes Module 下不同入口。

所以：

```
cmd/
```

就是：

> **Executable Entry（程序入口）**

---

## ② internal/

Go 官方最推荐。

里面代码：

**只能本 Module 使用。**

例如：

```
internal/

    service/

    database/

    cache/
```

别人：

```
import "github.com/xxx/project/internal/service"
```

编译失败。

所以：

internal 的心智模式：

> **Private Package**

类似：

```
Java private

C++ anonymous namespace

Rust(crate)
```

但是作用于 Package。

---

## ③ pkg/

这里放：

> 希望别人 import 的公共 Package。

例如：

```
pkg/logger

pkg/errors

pkg/httpclient
```

别人可以：

```
import "github.com/me/project/pkg/logger"
```

所以：

pkg：

就是：

> **Reusable Library**

---

## ④ api/

放：

```
OpenAPI

Swagger

protobuf

grpc
```

例如：

```
api/

    proto/

    swagger/
```

因为：

API 是一种契约。

不是业务代码。

---

## ⑤ configs/

放：

```
yaml

toml

json

ini
```

例如：

```
configs/

    dev.yaml

    prod.yaml
```

不是：

```
config.go
```

配置代码属于 package。

配置文件属于 configs。

---

## ⑥ scripts/

放：

```
build.sh

deploy.sh

gen.sh
```

只放：

脚本。

---

## ⑦ docs/

文档。

例如：

```
architecture.md

api.md

design.md
```

---

## ⑧ test/

大型项目会放：

```
integration test

fixtures

mock data
```

---

# 六、Package 如何划分？

这是最重要的问题。

不是按：

```
utils

common

helper
```

而是：

按职责划分。

例如：

电商：

```
user

order

payment

inventory
```

而不是：

```
stringutil

timeutil

mathutil
```

为什么？

因为：

Package 表示：

> **业务边界（Business Boundary）**

不是工具分类。

---

# 七、Go 最佳实践：不要出现 util 地狱

很多新手：

```
utils/

helper/

common/

misc/

base/
```

最后：

```
utils/

    string.go

    time.go

    file.go

    json.go

    http.go

    sql.go
```

5000 行。

没人知道放哪里。

最佳实践：

把工具放到真正使用它的 Package。

例如：

错误：

```
utils/json.go
```

正确：

```
config/parser.go

api/json.go

storage/codec.go
```

工具应该属于领域。

不是属于工具箱。

---

# 八、internal 与 pkg 如何选择？

原则：

如果：

```
未来别人不会 import
```

放：

```
internal/
```

如果：

```
希望别人复用
```

放：

```
pkg/
```

经验：

90%

都是：

```
internal/
```

只有真正 Library：

```
pkg/
```

---

# 九、推荐分层架构（Web 服务）

```
cmd/
    │
    ▼
handler
    │
    ▼
service
    │
    ▼
repository
    │
    ▼
database
```

职责：

```
handler
    HTTP

service
    Business

repository
    CRUD

database
    SQL
```

例如：

```
HTTP Request

      │

handler.Login()

      │

service.Login()

      │

repo.FindUser()

      │

db.Query()
```

职责非常清晰。

---

# 十、Interface 应该放哪里？

很多人：

```
interfaces/

    user.go

    order.go
```

这是 Java 思维。

Go：

接口应该放：

**消费者（Consumer）所在 Package。**

例如：

```
service/

    user.go

    repository.go
```

```
type UserRepository interface {

    Find(id int) (*User,error)

}
```

实现：

```
mysql/

    repository.go
```

这样：

```
service

↓

interface

↓

mysql
```

依赖方向永远正确。

口诀：

> **Interface belongs to the caller, not the implementer.**

---

# 十一、Go 项目依赖方向

推荐：

```
UI

↓

Handler

↓

Service

↓

Repository

↓

Infrastructure
```

永远不要：

```
Repository

↓

Service
```

反向依赖。

---

# 十二、Go 项目的演化过程

### 第一阶段

```
main.go
```

一个文件。

---

### 第二阶段

```
main.go

config.go

db.go
```

几个文件。

---

### 第三阶段

```
config/

service/

db/
```

Package。

---

### 第四阶段

```
cmd/

internal/

pkg/
```

大型工程。

---

### 第五阶段

```
DDD

Clean Architecture

Hexagonal
```

依赖 Interface。

---

# 十三、推荐目录（企业级）

```
project/
│
├── go.mod
├── go.sum
│
├── cmd/
│   ├── server/
│   │   └── main.go
│   └── cli/
│       └── main.go
│
├── internal/
│   ├── app/              # 应用启动、依赖注入
│   ├── config/           # 配置加载
│   ├── handler/          # HTTP / gRPC Handler
│   ├── service/          # 业务逻辑
│   ├── repository/       # 数据访问接口
│   ├── persistence/      # MySQL、Redis 等实现
│   ├── domain/           # 领域模型（可选，DDD）
│   ├── middleware/       # 中间件
│   └── transport/        # HTTP、gRPC、MQ 等适配器
│
├── pkg/
│   ├── logger/
│   ├── retry/
│   └── errors/
│
├── api/
│   ├── proto/
│   └── openapi/
│
├── configs/
├── scripts/
├── docs/
├── test/
└── README.md
```

---

# 十四、Go 项目组织的 80/20 心智模式

## 1. Package 是能力边界（Capability Boundary）

```
Package ≠ Folder

Package = 一个独立能力
```

例如：

```
user
order
payment
```

而不是：

```
utils
helper
misc
```

---

## 2. Directory 是依赖边界（Dependency Boundary）

目录表达的是：

```
谁可以依赖谁
```

而不是：

```
谁放在哪里更整齐
```

---

## 3. Interface 是契约（Contract）

```
Caller

↓

Interface

↓

Implementation
```

接口属于消费者，解耦调用方和实现方。

---

## 4. internal 是私有边界

```
internal

↓

只能自己 Module 用
```

用于保护内部实现，避免被外部误用。

---

## 5. cmd 是程序入口

```
一个程序

↓

一个 main.go
```

多个可执行程序对应多个 `cmd/<name>`。

---

## 6. 分层体现依赖方向

```
Handler
    ↓
Service
    ↓
Repository
    ↓
Infrastructure
```

每层只依赖下一层，避免循环依赖。

---

## 7. 高内聚、低耦合

每个 Package 只负责一件事情，对外暴露最小接口，把实现细节隐藏在包内。

---

# 十五、最佳实践 Checklist

| 实践 | 建议 |
|------|------|
| Package 划分 | 按业务能力或职责，而不是按文件类型 |
| `internal/` | 默认选择，存放绝大多数内部代码 |
| `pkg/` | 仅放明确需要被其他 Module 复用的公共库 |
| `cmd/` | 每个可执行程序一个独立入口 |
| Interface | 定义在调用方（消费者）所在 Package |
| 工具代码 | 避免 `utils/common/helper` 大杂烩，优先归属到具体领域 |
| 依赖关系 | 保持单向流动，杜绝循环依赖 |
| 配置与脚本 | 配置文件放 `configs/`，脚本放 `scripts/`，与业务代码分离 |
| 演进策略 | 从简单开始，随着复杂度增加逐步拆分 Package，而不是一开始过度设计 |

---

# 十六、一张图总结

```text
                    Go Project

                  ┌──────────────┐
                  │    Module    │
                  └──────┬───────┘
                         │
        ┌────────────────┴────────────────┐
        │                                 │
      cmd/                           internal/
        │                                 │
     main.go                   Package（业务能力）
                                        │
         ┌──────────────┬───────────────┴──────────────┐
         │              │              │               │
     handler        service      repository     infrastructure
         │              │              │               │
         └──────────────┴──────────────┴───────────────┘
                         │
                     Interface（契约）
                         │
                 单向依赖、高内聚、低耦合

        pkg/      → 可复用公共库
        api/      → 对外接口契约（OpenAPI、Proto）
        configs/  → 配置文件
        scripts/  → 构建与部署脚本
        docs/     → 设计文档
```

> **最终心智模型：**
>
> - **Module** 决定版本和发布边界。
> - **Package** 决定能力和职责边界。
> - **Directory** 表达依赖方向和可见性。
> - **Interface** 定义协作契约。
> - **Architecture（分层 / Clean / DDD）** 决定系统演化能力。
>
> 因此，一个优秀的 Go 项目组织，本质上是在通过目录结构和 Package 边界清晰地表达系统的职责划分、依赖方向和长期可维护性。