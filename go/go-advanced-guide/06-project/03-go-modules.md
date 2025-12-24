# Go Modules: go.mod and Versioning

## 1. Engineering Problem

**Go modules provide dependency management with reproducible builds.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GO MODULES                                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   go.mod:                          go.sum:                              │
│   ───────                          ───────                              │
│   module myproject                 • Checksums for modules              │
│   go 1.21                          • Ensures reproducibility            │
│   require (                        • Commit to version control          │
│       google.golang.org/grpc       │                                    │
│   )                                                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Key Commands

```bash
# Initialize module
go mod init github.com/company/project

# Add dependency
go get google.golang.org/grpc@v1.58.0

# Update dependencies
go get -u ./...

# Clean up
go mod tidy

# Vendor dependencies
go mod vendor
```

## 3. Version Selection

```go
// go.mod
module myproject

go 1.21

require (
    google.golang.org/grpc v1.58.0
    github.com/sirupsen/logrus v1.9.0
)
```

## 4. Replace Directive

```go
// For local development
replace github.com/company/lib => ../lib

// For forks
replace github.com/original/pkg => github.com/fork/pkg v1.0.0
```

---

## Chinese Explanation (中文解释)

### go.mod 文件

- 模块路径和 Go 版本
- 依赖列表和版本
- replace 指令（开发/fork）

### 关键命令

| 命令 | 作用 |
|------|------|
| `go mod init` | 初始化模块 |
| `go get` | 添加依赖 |
| `go mod tidy` | 清理依赖 |

