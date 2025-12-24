# Dependency Injection: Constructor Injection in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**Dependency injection makes code testable by allowing mock implementations to replace real ones.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DEPENDENCY INJECTION                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without DI:                        With DI:                           │
│   ───────────                        ────────                           │
│                                                                         │
│   type Service struct {}             type Service struct {              │
│                                          store RouteStore  // injected  │
│   func (s *Service) Get() {          }                                  │
│       db := sql.Open(...)  // hard   func NewService(store RouteStore)  │
│       db.Query(...)        // coded  func (s *Service) Get() {          │
│   }                                      s.store.Get(...)  // uses DI   │
│                                      }                                  │
│   Problems:                                                             │
│   • Can't test without DB            Benefits:                          │
│   • Can't swap implementations       • Easy to test with mocks          │
│   • Tightly coupled                  • Swap implementations             │
│                                      • Loosely coupled                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Code becomes untestable (requires real database, network, etc.)
- Components tightly coupled to specific implementations
- Changing one dependency requires changing many files
- Using DI frameworks adds unnecessary complexity

---

## 2. Core Mental Model

### How Go expects you to think

**Pass dependencies explicitly through constructors. No magic, no frameworks.**

```go
// Define what you need (interface)
type RouteStore interface {
    Get(key string) (Route, error)
    Add(route Route) error
}

// Accept interface in constructor
func NewRouteService(store RouteStore) *RouteService {
    return &RouteService{store: store}
}

// Use the dependency
func (s *RouteService) GetRoute(key string) (Route, error) {
    return s.store.Get(key)
}
```

### Philosophy

- Explicit is better than implicit
- No reflection, no configuration files
- Dependencies visible in constructor signature
- Easy to trace what depends on what

### Constructor injection vs other patterns

| Pattern | Go Approach |
|---------|-------------|
| Constructor injection | ✅ Primary pattern |
| Setter injection | ❌ Avoid (mutable state) |
| Field injection | ❌ Avoid (needs reflection) |
| Service locator | ❌ Avoid (hides dependencies) |

---

## 3. Language Mechanism

### Define interface at consumer

```go
// In route package - defines what IT needs
package route

type Store interface {
    Get(key string) (Route, error)
    Add(Route) error
}

type Service struct {
    store Store  // Uses local interface
}

func NewService(store Store) *Service {
    return &Service{store: store}
}
```

### Implement interface elsewhere

```go
// In storage package - implements the interface
package storage

type MemoryStore struct {
    routes map[string]route.Route
}

func NewMemoryStore() *MemoryStore {
    return &MemoryStore{routes: make(map[string]route.Route)}
}

func (m *MemoryStore) Get(key string) (route.Route, error) {
    r, ok := m.routes[key]
    if !ok {
        return route.Route{}, route.ErrNotFound
    }
    return r, nil
}

func (m *MemoryStore) Add(r route.Route) error {
    m.routes[r.Key()] = r
    return nil
}
```

### Wire dependencies in main

```go
package main

func main() {
    // Create concrete implementations
    store := storage.NewMemoryStore()
    logger := log.New(os.Stdout, "", log.LstdFlags)
    
    // Inject into service
    service := route.NewService(store)
    
    // Inject into handler
    handler := api.NewHandler(service, logger)
    
    // Start server
    http.Handle("/routes", handler)
    http.ListenAndServe(":8080", nil)
}
```

---

## 4. Idiomatic Usage

### When to use

- Components that need testing in isolation
- Swappable implementations (memory vs database)
- Cross-cutting concerns (logging, metrics)
- External service clients

### When NOT to use

- Simple utility functions
- Internal helpers that won't be mocked
- Types without behavior (data containers)

### Pattern: Functional options with DI

```go
type RouteService struct {
    store   RouteStore
    logger  *log.Logger
    timeout time.Duration
}

type Option func(*RouteService)

func WithLogger(l *log.Logger) Option {
    return func(s *RouteService) {
        s.logger = l
    }
}

func WithTimeout(d time.Duration) Option {
    return func(s *RouteService) {
        s.timeout = d
    }
}

func NewRouteService(store RouteStore, opts ...Option) *RouteService {
    s := &RouteService{
        store:   store,
        logger:  log.Default(),     // default
        timeout: 30 * time.Second,  // default
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Interface defined by implementer

```go
// BAD: Storage package defines interface
package storage

type Store interface {  // Defines 20 methods
    Get() ...
    Add() ...
    Delete() ...
    // ... many more
}

// Service must depend on storage package
import "storage"

// GOOD: Consumer defines interface
package route

type Store interface {  // Only what service needs
    Get(key string) (Route, error)
    Add(Route) error
}
```

### Pitfall 2: Injecting too much

```go
// BAD: Constructor takes 10 dependencies
func NewService(
    store Store,
    cache Cache,
    logger Logger,
    metrics Metrics,
    tracer Tracer,
    config Config,
    validator Validator,
    // ... more
) *Service

// GOOD: Group related dependencies
type ServiceDeps struct {
    Store   Store
    Logger  *log.Logger
    Metrics MetricsClient
}

func NewService(deps ServiceDeps) *Service
```

### Pitfall 3: Passing context through DI

```go
// BAD: Context as dependency
func NewService(ctx context.Context, store Store) *Service

// GOOD: Context passed per-request
func NewService(store Store) *Service

func (s *Service) Get(ctx context.Context, key string) (Route, error) {
    return s.store.Get(ctx, key)
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "sync"
    "time"
)

// ==========================================
// Domain types
// ==========================================

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func (r Route) Key() string {
    return fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
}

var ErrNotFound = errors.New("route not found")

// ==========================================
// Interface defined by consumer
// ==========================================

type RouteStore interface {
    Get(ctx context.Context, key string) (Route, error)
    Add(ctx context.Context, route Route) error
    List(ctx context.Context) ([]Route, error)
}

// ==========================================
// Service with injected dependencies
// ==========================================

type RouteService struct {
    store  RouteStore
    logger *log.Logger
}

func NewRouteService(store RouteStore, logger *log.Logger) *RouteService {
    return &RouteService{
        store:  store,
        logger: logger,
    }
}

func (s *RouteService) GetRoute(ctx context.Context, vrfID uint32, prefix string) (Route, error) {
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    s.logger.Printf("Getting route: %s", key)
    
    route, err := s.store.Get(ctx, key)
    if err != nil {
        s.logger.Printf("Error getting route %s: %v", key, err)
        return Route{}, err
    }
    
    return route, nil
}

func (s *RouteService) AddRoute(ctx context.Context, route Route) error {
    s.logger.Printf("Adding route: %s", route.Key())
    return s.store.Add(ctx, route)
}

// ==========================================
// Concrete implementation: Memory store
// ==========================================

type MemoryStore struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewMemoryStore() *MemoryStore {
    return &MemoryStore{
        routes: make(map[string]Route),
    }
}

func (m *MemoryStore) Get(ctx context.Context, key string) (Route, error) {
    m.mu.RLock()
    defer m.mu.RUnlock()
    
    route, ok := m.routes[key]
    if !ok {
        return Route{}, ErrNotFound
    }
    return route, nil
}

func (m *MemoryStore) Add(ctx context.Context, route Route) error {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    m.routes[route.Key()] = route
    return nil
}

func (m *MemoryStore) List(ctx context.Context) ([]Route, error) {
    m.mu.RLock()
    defer m.mu.RUnlock()
    
    routes := make([]Route, 0, len(m.routes))
    for _, r := range m.routes {
        routes = append(routes, r)
    }
    return routes, nil
}

// ==========================================
// Mock implementation for testing
// ==========================================

type MockStore struct {
    GetFunc  func(ctx context.Context, key string) (Route, error)
    AddFunc  func(ctx context.Context, route Route) error
    ListFunc func(ctx context.Context) ([]Route, error)
}

func (m *MockStore) Get(ctx context.Context, key string) (Route, error) {
    if m.GetFunc != nil {
        return m.GetFunc(ctx, key)
    }
    return Route{}, nil
}

func (m *MockStore) Add(ctx context.Context, route Route) error {
    if m.AddFunc != nil {
        return m.AddFunc(ctx, route)
    }
    return nil
}

func (m *MockStore) List(ctx context.Context) ([]Route, error) {
    if m.ListFunc != nil {
        return m.ListFunc(ctx)
    }
    return nil, nil
}

// ==========================================
// Testing with mock
// ==========================================

func TestRouteService_GetRoute(t *testing.T) {
    // Create mock
    mock := &MockStore{
        GetFunc: func(ctx context.Context, key string) (Route, error) {
            if key == "1:10.0.0.0/24" {
                return Route{VrfID: 1, Prefix: "10.0.0.0/24"}, nil
            }
            return Route{}, ErrNotFound
        },
    }
    
    // Inject mock
    logger := log.New(io.Discard, "", 0)
    service := NewRouteService(mock, logger)
    
    // Test
    route, err := service.GetRoute(context.Background(), 1, "10.0.0.0/24")
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
    if route.Prefix != "10.0.0.0/24" {
        t.Errorf("got %v, want 10.0.0.0/24", route.Prefix)
    }
}

// ==========================================
// Main: wire everything together
// ==========================================

func main() {
    // Create concrete implementations
    store := NewMemoryStore()
    logger := log.New(os.Stdout, "[route] ", log.LstdFlags)
    
    // Inject dependencies
    service := NewRouteService(store, logger)
    
    // Use service
    ctx := context.Background()
    
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    if err := service.AddRoute(ctx, route); err != nil {
        log.Fatal(err)
    }
    
    r, err := service.GetRoute(ctx, 1, "10.0.0.0/24")
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Got route: %+v\n", r)
}

// Need to add these imports for full compilation
import (
    "io"
    "os"
    "testing"
)
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DEPENDENCY INJECTION RULES                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. CONSTRUCTOR INJECTION                                              │
│      • Pass dependencies via NewXxx()                                   │
│      • No setter injection                                              │
│      • No field injection                                               │
│                                                                         │
│   2. DEFINE INTERFACES AT CONSUMER                                      │
│      • Consumer knows what it needs                                     │
│      • Keep interfaces small                                            │
│      • Implementer doesn't need to know                                 │
│                                                                         │
│   3. NO FRAMEWORKS                                                      │
│      • Wire in main()                                                   │
│      • Explicit is better                                               │
│      • Dependencies visible in code                                     │
│                                                                         │
│   4. EASY TESTING                                                       │
│      • Create mock implementing interface                               │
│      • Inject mock in tests                                             │
│      • No real database/network needed                                  │
│                                                                         │
│   5. CONTEXT PASSED PER-REQUEST                                         │
│      • Not injected in constructor                                      │
│      • Passed to each method                                            │
│                                                                         │
│   6. GROUP RELATED DEPENDENCIES                                         │
│      • Use struct for many deps                                         │
│      • Use options for optional deps                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 依赖注入核心概念

**通过构造函数显式传递依赖。无框架、无魔法。**

### 基本模式

```go
// 定义接口（消费者侧）
type Store interface {
    Get(key string) (Route, error)
}

// 构造函数接收依赖
func NewService(store Store) *Service {
    return &Service{store: store}
}
```

### 测试中使用 Mock

```go
mock := &MockStore{
    GetFunc: func(key string) (Route, error) {
        return Route{...}, nil
    },
}
service := NewService(mock)
```

### 最佳实践

1. **构造函数注入**：不用 setter 或字段注入
2. **消费者定义接口**：接口在使用方定义
3. **无框架**：在 main() 中手动组装
4. **Context 按请求传递**：不在构造函数中注入
5. **分组相关依赖**：依赖多时用结构体

### 常见陷阱

- 接口由实现者定义（应由消费者定义）
- 注入太多依赖（应该分组）
- Context 作为依赖注入（应按请求传递）
