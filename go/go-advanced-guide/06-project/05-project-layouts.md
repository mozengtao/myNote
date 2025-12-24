# Project Layouts: /cmd, /internal, /pkg

## 1. Standard Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PROJECT STRUCTURE                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   myproject/                                                            │
│   ├── cmd/                    # Main applications                       │
│   │   ├── server/                                                       │
│   │   │   └── main.go        # Entry point                             │
│   │   └── cli/                                                          │
│   │       └── main.go                                                   │
│   │                                                                     │
│   ├── internal/               # Private code (Go enforced)             │
│   │   ├── route/                                                        │
│   │   ├── config/                                                       │
│   │   └── server/                                                       │
│   │                                                                     │
│   ├── pkg/                    # Public library (if any)                │
│   │   └── api/                                                          │
│   │                                                                     │
│   ├── api/                    # API definitions (proto, OpenAPI)        │
│   ├── scripts/                # Build/deploy scripts                    │
│   ├── testdata/               # Test fixtures                           │
│   │                                                                     │
│   ├── go.mod                                                            │
│   └── go.sum                                                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. cmd/ Structure

```go
// cmd/server/main.go
package main

func main() {
    cfg := config.Load()
    srv := server.New(cfg)
    srv.Run()
}
```

## 3. internal/ Usage

```go
// internal/route/route.go
package route

type Route struct { ... }
func NewRoute() *Route { ... }

// Only importable from within the project
```

## 4. When to Use /pkg

- Rarely needed
- Only for truly reusable library code
- Most projects use only /cmd and /internal

---

## Chinese Explanation (中文解释)

### 目录用途

| 目录 | 用途 |
|------|------|
| cmd/ | 主程序入口 |
| internal/ | 私有代码 |
| pkg/ | 公共库（少用） |
| api/ | API 定义 |

