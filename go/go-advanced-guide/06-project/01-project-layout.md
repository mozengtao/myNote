# Project Layout and Package Design

## 1. Engineering Problem

### What real-world problem does this solve?

**Good project structure scales with team size and codebase growth.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STANDARD GO PROJECT LAYOUT                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   myproject/                                                            │
│   ├── cmd/                      # Main applications                     │
│   │   ├── server/                                                       │
│   │   │   └── main.go           # Server entry point                    │
│   │   └── cli/                                                          │
│   │       └── main.go           # CLI entry point                       │
│   │                                                                     │
│   ├── internal/                 # Private code (not importable)         │
│   │   ├── route/                # Route business logic                  │
│   │   ├── store/                # Storage layer                         │
│   │   └── config/               # Configuration                         │
│   │                                                                     │
│   ├── pkg/                      # Public library code (if needed)       │
│   │   └── api/                  # API definitions                       │
│   │                                                                     │
│   ├── api/                      # API definitions (protobuf, OpenAPI)   │
│   ├── web/                      # Static web assets                     │
│   ├── scripts/                  # Build/deploy scripts                  │
│   ├── testdata/                 # Test fixtures                         │
│   │                                                                     │
│   ├── go.mod                    # Module definition                     │
│   ├── go.sum                    # Dependency checksums                  │
│   └── README.md                                                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Package principles

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PACKAGE DESIGN                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PACKAGE BY FEATURE, NOT LAYER                                      │
│                                                                         │
│   BAD (Java-style):              GOOD (Go-style):                       │
│   ─────────────────              ───────────────                        │
│   models/                        route/                                 │
│     route.go                       route.go       # type + logic        │
│     vmc.go                         store.go       # persistence         │
│   controllers/                     handler.go     # HTTP handler        │
│     route.go                     vmc/                                   │
│     vmc.go                         vmc.go                               │
│   services/                        store.go                             │
│     route.go                                                            │
│     vmc.go                                                              │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   2. NAMING CONVENTIONS                                                 │
│                                                                         │
│   • Package: lowercase, single word if possible                         │
│   • Exported: PascalCase (RouteManager)                                 │
│   • Unexported: camelCase (routeStore)                                  │
│   • Don't repeat package in name: route.Route not route.RouteRoute      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   3. DEPENDENCY DIRECTION                                               │
│                                                                         │
│   cmd/server ──► internal/route ──► internal/store                      │
│       │              │                   │                              │
│       │              ▼                   ▼                              │
│       └──────► internal/config ◄──── database                          │
│                                                                         │
│   Higher layers depend on lower. Never circular.                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### internal/ directory

```go
// internal/ is enforced by the Go toolchain
// Code in internal/ can only be imported by code in parent

myproject/
├── internal/
│   └── secret/
│       └── secret.go    // package secret

// Can import:
// - myproject/cmd/server
// - myproject/internal/other

// CANNOT import:
// - external/project (blocked by Go)
```

### Package visibility

```go
package route

// Exported (uppercase) - visible outside package
type Route struct {
    VrfID   uint32
    Prefix  string
}

func NewRoute(prefix string) *Route { ... }

// Unexported (lowercase) - package-private
type routeStore struct { ... }

func validate(r *Route) error { ... }
```

### Go modules

```go
// go.mod
module github.com/mycompany/routermgr

go 1.21

require (
    google.golang.org/grpc v1.58.0
    github.com/sirupsen/logrus v1.9.0
)

// go.sum contains checksums (auto-generated)
```

---

## 4. Idiomatic Usage

### cmd/ structure

```go
// cmd/server/main.go
package main

import (
    "log"
    "os"
    
    "github.com/mycompany/routermgr/internal/config"
    "github.com/mycompany/routermgr/internal/server"
)

func main() {
    cfg, err := config.Load(os.Getenv("CONFIG_PATH"))
    if err != nil {
        log.Fatal(err)
    }
    
    srv := server.New(cfg)
    if err := srv.Run(); err != nil {
        log.Fatal(err)
    }
}
```

### internal/ packages

```go
// internal/route/route.go
package route

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type Store interface {
    Get(key string) (*Route, error)
    Add(r *Route) error
    Delete(key string) error
}

// internal/route/memory.go
package route

type memoryStore struct {
    routes map[string]*Route
}

func NewMemoryStore() Store {
    return &memoryStore{routes: make(map[string]*Route)}
}
```

### Dependency injection

```go
// internal/server/server.go
package server

type Server struct {
    routes route.Store
    config *config.Config
    log    *log.Logger
}

func New(cfg *config.Config) *Server {
    return &Server{
        routes: route.NewMemoryStore(),
        config: cfg,
        log:    log.Default(),
    }
}

// For testing
func NewWithDeps(routes route.Store, cfg *config.Config, log *log.Logger) *Server {
    return &Server{routes: routes, config: cfg, log: log}
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Circular imports

```go
// BAD: Circular dependency
// package route imports store
// package store imports route

// route/route.go
package route
import "myproject/store"

// store/store.go  
package store
import "myproject/route"  // ERROR: import cycle

// FIX: Use interfaces to break cycle
// route/interfaces.go
package route

type Store interface {
    Save(r Route) error
}

// store/store.go
package store

import "myproject/route"

type RouteStore struct{}

func (s *RouteStore) Save(r route.Route) error { ... }
```

### Pitfall 2: Overly generic package names

```go
// BAD
package util
package common
package helper

// GOOD
package stringutil
package httputil
package testutil
```

### Pitfall 3: Deep nesting

```go
// BAD: Too many levels
internal/services/route/v1/api/handlers/route.go

// GOOD: Flat structure
internal/route/handler.go
```

---

## 6. Complete Example

```go
// Project structure:
// routermgr/
// ├── cmd/
// │   └── server/
// │       └── main.go
// ├── internal/
// │   ├── config/
// │   │   └── config.go
// │   ├── route/
// │   │   ├── route.go
// │   │   ├── store.go
// │   │   └── handler.go
// │   └── server/
// │       └── server.go
// ├── go.mod
// └── go.sum

// --- cmd/server/main.go ---
package main

import (
    "context"
    "log"
    "os"
    "os/signal"
    "syscall"
    
    "routermgr/internal/config"
    "routermgr/internal/server"
)

func main() {
    cfg, err := config.Load()
    if err != nil {
        log.Fatalf("config: %v", err)
    }
    
    srv := server.New(cfg)
    
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()
    
    go func() {
        stop := make(chan os.Signal, 1)
        signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
        <-stop
        cancel()
    }()
    
    if err := srv.Run(ctx); err != nil {
        log.Fatalf("server: %v", err)
    }
}

// --- internal/config/config.go ---
package config

import (
    "os"
)

type Config struct {
    Port    string
    LogLevel string
}

func Load() (*Config, error) {
    return &Config{
        Port:     getEnv("PORT", ":8080"),
        LogLevel: getEnv("LOG_LEVEL", "info"),
    }, nil
}

func getEnv(key, fallback string) string {
    if v := os.Getenv(key); v != "" {
        return v
    }
    return fallback
}

// --- internal/route/route.go ---
package route

type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

func (r Route) Key() string {
    return fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
}

// --- internal/route/store.go ---
package route

import (
    "errors"
    "sync"
)

var ErrNotFound = errors.New("route not found")

type Store interface {
    Get(key string) (Route, error)
    Add(r Route) error
    List() []Route
}

type memoryStore struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewMemoryStore() Store {
    return &memoryStore{routes: make(map[string]Route)}
}

func (s *memoryStore) Get(key string) (Route, error) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    r, ok := s.routes[key]
    if !ok {
        return Route{}, ErrNotFound
    }
    return r, nil
}

func (s *memoryStore) Add(r Route) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.routes[r.Key()] = r
    return nil
}

func (s *memoryStore) List() []Route {
    s.mu.RLock()
    defer s.mu.RUnlock()
    result := make([]Route, 0, len(s.routes))
    for _, r := range s.routes {
        result = append(result, r)
    }
    return result
}

// --- internal/server/server.go ---
package server

import (
    "context"
    "encoding/json"
    "log"
    "net/http"
    
    "routermgr/internal/config"
    "routermgr/internal/route"
)

type Server struct {
    config *config.Config
    routes route.Store
    http   *http.Server
}

func New(cfg *config.Config) *Server {
    s := &Server{
        config: cfg,
        routes: route.NewMemoryStore(),
    }
    
    mux := http.NewServeMux()
    mux.HandleFunc("/routes", s.handleRoutes)
    mux.HandleFunc("/health", s.handleHealth)
    
    s.http = &http.Server{
        Addr:    cfg.Port,
        Handler: mux,
    }
    
    return s
}

func (s *Server) Run(ctx context.Context) error {
    go func() {
        <-ctx.Done()
        s.http.Shutdown(context.Background())
    }()
    
    log.Printf("Starting server on %s", s.config.Port)
    return s.http.ListenAndServe()
}

func (s *Server) handleRoutes(w http.ResponseWriter, r *http.Request) {
    switch r.Method {
    case http.MethodGet:
        json.NewEncoder(w).Encode(s.routes.List())
    case http.MethodPost:
        var route route.Route
        json.NewDecoder(r.Body).Decode(&route)
        s.routes.Add(route)
        w.WriteHeader(http.StatusCreated)
    }
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
    w.Write([]byte("OK"))
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PROJECT LAYOUT RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. cmd/ FOR BINARIES                                                  │
│      • Each subdirectory = one binary                                   │
│      • Minimal code, mainly wiring                                      │
│                                                                         │
│   2. internal/ FOR PRIVATE CODE                                         │
│      • Enforced by Go toolchain                                         │
│      • Cannot be imported externally                                    │
│                                                                         │
│   3. pkg/ ONLY IF TRULY PUBLIC                                          │
│      • Most projects don't need it                                      │
│      • Use for reusable library code                                    │
│                                                                         │
│   4. PACKAGE BY FEATURE                                                 │
│      • Not by layer (models/, controllers/)                             │
│      • Keep related code together                                       │
│                                                                         │
│   5. AVOID CIRCULAR IMPORTS                                             │
│      • Use interfaces to break cycles                                   │
│      • Dependency direction: high → low                                 │
│                                                                         │
│   6. KEEP IT FLAT                                                       │
│      • Avoid deep nesting                                               │
│      • Refactor when packages grow                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 标准目录结构

| 目录 | 用途 |
|------|------|
| cmd/ | 主程序入口 |
| internal/ | 私有代码（不可外部导入） |
| pkg/ | 公共库代码（如需要） |
| api/ | API 定义（protobuf, OpenAPI） |
| testdata/ | 测试数据 |

### 包设计原则

1. **按功能分包**：不是按层（models/, controllers/）
2. **保持扁平**：避免深层嵌套
3. **避免循环导入**：用接口打破循环
4. **依赖方向**：高层依赖低层

### 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 包名 | 小写单词 | route, config |
| 导出类型 | PascalCase | RouteManager |
| 未导出 | camelCase | routeStore |

### internal/ 目录

- Go 工具链强制执行
- 只能被父目录导入
- 外部包无法导入

### 最佳实践

- cmd/ 每个子目录一个二进制
- internal/ 放私有代码
- 按功能而非层分包
- 保持结构扁平

