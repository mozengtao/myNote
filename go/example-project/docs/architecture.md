# 架构说明

本文档配合仓库根目录 [go/project_organization.md](../../project_organization.md) 阅读，
用这个可运行的用户管理服务把文档里的心智模型逐条落地。

## 依赖方向

```
cmd/server (main.go)
        │
        ▼
internal/app (依赖注入 + 生命周期)
        │
        ▼
internal/transport/http (路由 + 中间件)
        │
        ▼
internal/handler (HTTP 适配层)
        │
        ▼
internal/service (业务逻辑)
        │
        ▼
internal/repository (UserRepository 接口)
        ▲
        │ 实现（反向依赖）
        │
   ┌────┴─────┐
   │          │
persistence/  persistence/
  memory        mysql
```

依赖只有一个方向：上层依赖下层的**接口**，下层的具体实现反过来实现这个接口。
`internal/service` 永远不会 import `internal/persistence/memory` 或
`internal/persistence/mysql`，它只认识 `internal/repository.UserRepository`。

## 每一层的职责

| 层 | 目录 | 职责 | 依赖 |
|----|------|------|------|
| 入口 | `cmd/server` | 解析命令行参数，调用 `app.Run` | `internal/app`, `internal/config` |
| 组装 | `internal/app` | 依赖注入、选择 repository 实现、启动/优雅关闭 HTTP 服务器 | 下面所有层 |
| 传输 | `internal/transport/http` | 注册路由、串联中间件 | `internal/handler`, `internal/middleware` |
| 横切关注点 | `internal/middleware` | 日志、panic 恢复 | `pkg/logger` |
| 适配 | `internal/handler` | 解析/编码 HTTP、把错误翻译成状态码 | `internal/service`, `pkg/errors` |
| 业务 | `internal/service` | 用例编排、密码哈希、生成 token | `internal/repository`, `internal/domain` |
| 契约 | `internal/repository` | 定义 `UserRepository` 接口 | `internal/domain` |
| 实现 | `internal/persistence/*` | 具体存储实现（内存 / MySQL 占位） | `internal/repository`, `internal/domain` |
| 领域 | `internal/domain` | 领域模型与哨兵错误 | 无（最底层，谁都可以依赖它，它不依赖任何人） |
| 复用库 | `pkg/*` | 日志、错误边界、重试——不含业务知识，可被其他模块直接 import | 无内部依赖 |

## 关于 `internal/repository` 放接口这件事

`project_organization.md` 第十节主张"接口属于消费者所在的 package"（即接口应该定义
在 `service/repository.go` 里）。本项目遵循文档第十三节"企业级目录"的写法，把接口单独
放进 `internal/repository/` 包。两种做法都符合"接口属于调用方视角、实现方反向依赖接口"
这一核心原则；当接口未来可能被多个消费者（比如除 `service` 外还有后台任务）共用时，
独立出一个 `repository` 包可以避免这些消费者互相依赖对方所在的包。

## 错误边界（Error Boundary）

`internal/domain` 只定义 Go 原生的哨兵错误（`ErrUserNotFound` 等），`internal/service`
和 `internal/persistence/*` 都只处理/返回这些错误，完全不知道 HTTP 状态码的存在。
只有 `internal/handler`（系统边界）通过 `pkg/errors.AppError` 把它们翻译成
404/409/401/500，这样业务逻辑可以在完全不同的传输层（HTTP、gRPC、CLI）下复用。

## 可替换的持久化实现

`internal/persistence/memory` 是真实可用的线程安全内存实现，默认配置
（`configs/dev.json`）使用它，因此整个服务不依赖任何外部数据库就能跑起来。

`internal/persistence/mysql` 实现了同一个 `UserRepository` 接口，但方法体
只是 `fmt.Printf` 模拟 SQL 执行和一次连接重试（通过 `pkg/retry`），并不真正
连接数据库。它存在的意义是证明：只要实现了同一接口，`internal/app` 就可以
仅凭 `configs/*.json` 里的一行配置在两种实现之间切换，`service`/`handler`
不需要任何改动。
