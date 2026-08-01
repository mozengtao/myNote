# Go 项目组织最佳实践 —— 可运行示例项目

这是一个**真实可以 `go build` / `go run` / `go test`** 的最小用户管理 HTTP 服务，
目的是把 [go/project_organization.md](../project_organization.md)（尤其是第 850-885 行的
企业级推荐目录）里的每一条心智模型都用可运行的代码落地，而不是停留在空文件夹层面。

零第三方依赖：只使用 Go 标准库（`net/http`、`encoding/json`、`crypto/sha256` 等）。
凡是真实项目中会很复杂的部分（真正连接 MySQL、签发 JWT），都用 `fmt.Printf` 打印
"这里生产环境会做什么"来代替，详见下文"简化之处"。

## 快速开始

```bash
# 编译
./scripts/build.sh

# 用 configs/dev.json 直接运行（内存存储，无需任何外部依赖）
./scripts/run.sh

# 跑单元测试 + 集成测试
./scripts/test.sh
```

启动后可以用 curl 走一遍完整流程：

```bash
curl -s localhost:8080/healthz

curl -s -X POST localhost:8080/users/register \
  -d '{"username":"alice","password":"s3cret"}'

curl -s -X POST localhost:8080/users/login \
  -d '{"username":"alice","password":"s3cret"}'

curl -s localhost:8080/users/<上一步返回的 id>
```

## 目录结构

```
go/example-project/
├── go.mod
├── README.md
├── cmd/
│   └── server/            # 唯一可执行入口，只做参数解析 + app.Run()
├── internal/
│   ├── app/                # 依赖注入、组装各层、优雅关闭
│   ├── config/              # 加载 configs/*.json
│   ├── domain/               # User 模型 + 哨兵错误（最底层，零依赖）
│   ├── repository/            # UserRepository 接口（数据访问契约）
│   ├── persistence/
│   │   ├── memory/              # 真实实现：线程安全内存存储
│   │   └── mysql/                # 占位实现：打印模拟 SQL + 重试
│   ├── service/                    # 业务逻辑：Register/Login/GetUser
│   ├── handler/                     # HTTP handler，领域错误 -> HTTP 状态码
│   ├── middleware/                   # 日志、panic 恢复
│   └── transport/http/                # 路由 + 中间件组装
├── pkg/
│   ├── logger/    # 极简分级日志，可被其他模块直接复用
│   ├── retry/     # 通用重试 + 退避
│   └── errors/    # AppError：统一错误边界/HTTP 状态映射
├── api/openapi/     # 手写的 OpenAPI 契约文档
├── configs/           # dev.json / prod.json
├── scripts/            # build.sh / run.sh / test.sh
├── docs/architecture.md # 依赖方向图 + 分层职责表
└── test/integration/     # httptest 端到端测试
```

## 对照 project_organization.md 的原则

| 文档里的原则 | 本项目里的体现 |
|---|---|
| Package 是能力边界，不是文件类型 | `domain`/`service`/`handler`/`repository` 按职责划分，没有 `utils`/`helper` |
| `internal/` 是默认选择 | 除了 `pkg/logger`、`pkg/retry`、`pkg/errors`，其余全部在 `internal/` 下 |
| `pkg/` 只放明确要复用的库 | `pkg/*` 不包含任何 `User`/密码等业务概念 |
| `cmd/` 每个可执行程序一个入口 | 只有 `cmd/server`，`main.go` 里没有一行业务逻辑 |
| Interface 属于调用方 | `UserRepository` 定义在 `repository/`，`service` 依赖接口而不是具体实现；`persistence/memory`、`persistence/mysql` 反向实现它（见 [docs/architecture.md](docs/architecture.md) 里对这一取舍的详细说明） |
| 依赖单向流动 | `handler -> service -> repository(interface) <- persistence`，任何一层都不会 import 上层 |
| 配置与代码分离 | 配置代码在 `internal/config/config.go`，配置数据在 `configs/*.json` |
| 从简单开始，逐步拆分 | 见 `docs/architecture.md` 里对每一层职责的说明，对应文档"演化过程"第四、五阶段 |

## 简化之处（用打印代替的"复杂实现"）

- **MySQL 持久化**（`internal/persistence/mysql`）：没有引入真正的驱动，方法体用
  `fmt.Printf` 打印"会执行的 SQL"，并用 `pkg/retry` 模拟一次连接重试。它的作用
  只是证明"同一个 `UserRepository` 接口可以有多种实现，`internal/app` 靠配置切换"。
- **密码哈希**：用标准库 `crypto/sha256`，真实项目应使用 `bcrypt`/`argon2` 等
  专为密码设计的慢哈希算法（那属于第三方依赖，这里为了零依赖简化）。
- **登录 token**：`internal/service` 用 `crypto/rand` 生成一个随机十六进制字符串
  代替真正签发的 JWT，代码里打印了"这里生产环境应该签发一个真正的 JWT"。
- **YAML 配置**：`configs/*.json` 用 JSON 而不是 YAML，因为标准库没有内置 YAML
  解析器；真实项目通常会引入 `viper` 或 `gopkg.in/yaml.v3`。

## 运行测试

```bash
go test ./... -v
```

- `internal/service/user_service_test.go`：单元测试，与业务代码同目录（Go 惯用法），
  用 `persistence/memory` 当测试替身。
- `test/integration/user_api_test.go`：集成测试，真实拉起 `net/http` 服务器，
  走一遍注册 -> 重复注册冲突 -> 登录 -> 密码错误 -> 查询 -> 查询不存在的完整链路。
