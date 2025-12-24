# Anti-Patterns: What to Avoid in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**Recognizing anti-patterns prevents subtle bugs, performance issues, and maintenance nightmares.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    COMMON GO ANTI-PATTERNS                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. Global mutable state     → Race conditions, testing hell           │
│   2. Overusing channels       → Complexity, deadlocks                   │
│   3. Java-style abstractions  → Over-engineering, indirection           │
│   4. interface{} everywhere   → Lost type safety                        │
│   5. Not using defer          → Resource leaks                          │
│   6. Ignoring errors          → Silent failures                         │
│   7. Premature optimization   → Wasted effort, complex code             │
│   8. Giant interfaces         → Tight coupling                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong?

- Code becomes untestable and unmaintainable
- Subtle bugs appear under load
- Performance degrades unexpectedly
- New team members struggle to understand

---

## 2. Core Mental Model

### How Go expects you to think

**Simplicity over cleverness. Explicit over implicit. Composition over inheritance.**

Go's philosophy:
- Less is more
- Clear is better than clever
- A little copying is better than a little dependency
- Errors are values, handle them

---

## 3. Language Mechanism

### Anti-Pattern 1: Global Mutable State

```go
// =====================
// BAD: Global mutable state
// =====================
var cache = make(map[string]Result)  // Package-level mutable

func GetResult(key string) Result {
    if r, ok := cache[key]; ok {
        return r  // RACE CONDITION!
    }
    r := compute(key)
    cache[key] = r  // RACE CONDITION!
    return r
}

// Problems:
// • Data race between goroutines
// • Hard to test (global state persists)
// • Hard to reason about

// =====================
// GOOD: Encapsulated state
// =====================
type Cache struct {
    mu    sync.RWMutex
    items map[string]Result
}

func NewCache() *Cache {
    return &Cache{items: make(map[string]Result)}
}

func (c *Cache) Get(key string) (Result, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    r, ok := c.items[key]
    return r, ok
}

func (c *Cache) Set(key string, r Result) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.items[key] = r
}
```

### Anti-Pattern 2: Overusing Channels

```go
// =====================
// BAD: Channel for simple synchronization
// =====================
func waitForAll() {
    done := make(chan struct{})
    count := 0
    
    for i := 0; i < 10; i++ {
        go func() {
            doWork()
            done <- struct{}{}
        }()
    }
    
    for count < 10 {
        <-done
        count++
    }
}

// Problems:
// • Overly complex
// • Easy to get count wrong

// =====================
// GOOD: Use WaitGroup
// =====================
func waitForAll() {
    var wg sync.WaitGroup
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            doWork()
        }()
    }
    
    wg.Wait()
}

// Rule of thumb:
// • Channels for communication between goroutines
// • sync primitives for coordination/synchronization
```

### Anti-Pattern 3: Java-Style Abstractions

```go
// =====================
// BAD: Over-abstracted
// =====================
type IRouteService interface {
    GetRoute(key string) (Route, error)
    AddRoute(route Route) error
    DeleteRoute(key string) error
    UpdateRoute(route Route) error
    ListRoutes() ([]Route, error)
    // ... 20 more methods
}

type RouteServiceImpl struct {
    repository IRouteRepository
    validator  IRouteValidator
    cache      IRouteCache
    logger     ILogger
}

type RouteServiceFactory struct {
    // ...
}

func (f *RouteServiceFactory) CreateRouteService(config IConfig) IRouteService {
    // ...
}

// Problems:
// • Unnecessary indirection
// • Interfaces defined by implementer
// • Factory pattern rarely needed in Go

// =====================
// GOOD: Simple and direct
// =====================
type RouteService struct {
    store  RouteStore  // Small interface
    logger *log.Logger
}

func NewRouteService(store RouteStore) *RouteService {
    return &RouteService{
        store:  store,
        logger: log.Default(),
    }
}

func (s *RouteService) Get(key string) (Route, error) {
    return s.store.Get(key)
}
```

### Anti-Pattern 4: Empty Interface Everywhere

```go
// =====================
// BAD: Type safety lost
// =====================
func Process(data interface{}) interface{} {
    // Type assertions everywhere
    switch v := data.(type) {
    case Route:
        return processRoute(v)
    case Address:
        return processAddress(v)
    default:
        panic("unknown type")
    }
}

// Problems:
// • No compile-time type checking
// • Runtime panics
// • Hard to understand API

// =====================
// GOOD: Use generics or specific types
// =====================
// Option 1: Generics (Go 1.18+)
func Process[T Route | Address](data T) Result {
    // Type-safe
}

// Option 2: Specific functions
func ProcessRoute(r Route) RouteResult
func ProcessAddress(a Address) AddressResult
```

### Anti-Pattern 5: Not Using Defer

```go
// =====================
// BAD: Multiple return paths
// =====================
func process(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    
    data, err := io.ReadAll(f)
    if err != nil {
        f.Close()  // Must remember!
        return err
    }
    
    if err := validate(data); err != nil {
        f.Close()  // Must remember!
        return err
    }
    
    f.Close()  // Must remember!
    return nil
}

// =====================
// GOOD: Defer cleanup
// =====================
func process(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()  // Always runs
    
    data, err := io.ReadAll(f)
    if err != nil {
        return err
    }
    
    return validate(data)
}
```

---

## 4. Idiomatic Usage

### When patterns become anti-patterns

| Pattern | OK When | Anti-Pattern When |
|---------|---------|-------------------|
| Global var | Constants, singletons | Mutable shared state |
| Channel | Communication | Simple sync |
| Interface | Polymorphism needed | Every type |
| interface{} | Truly generic (json) | Specific types exist |

---

## 5. Common Pitfalls

### Pitfall 1: Init function abuse

```go
// BAD: Complex init
func init() {
    db, err := sql.Open("postgres", os.Getenv("DB_URL"))
    if err != nil {
        log.Fatal(err)  // Crashes before main!
    }
    // ...
}

// GOOD: Explicit initialization in main
func main() {
    db, err := sql.Open("postgres", os.Getenv("DB_URL"))
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()
    
    // ...
}
```

### Pitfall 2: Panic for errors

```go
// BAD: Panic for expected errors
func GetRoute(key string) Route {
    r, err := store.Get(key)
    if err != nil {
        panic(err)  // Crashes caller!
    }
    return r
}

// GOOD: Return error
func GetRoute(key string) (Route, error) {
    r, err := store.Get(key)
    if err != nil {
        return Route{}, fmt.Errorf("get route %s: %w", key, err)
    }
    return r, nil
}
```

### Pitfall 3: Naked returns

```go
// BAD: Naked return obscures what's returned
func calculate(x int) (result int, err error) {
    if x < 0 {
        err = errors.New("negative")
        return  // What's result? 0? Previous value?
    }
    result = x * 2
    return  // Same problem
}

// GOOD: Explicit returns
func calculate(x int) (int, error) {
    if x < 0 {
        return 0, errors.New("negative")
    }
    return x * 2, nil
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "errors"
    "fmt"
    "log"
    "sync"
)

// =====================
// ANTI-PATTERN CODE (DON'T DO THIS)
// =====================

// Anti-pattern 1: Global mutable state
var globalRoutes = make(map[string]Route)  // BAD!

// Anti-pattern 2: Giant interface
type IRouteManager interface {
    GetRoute(key string) (Route, error)
    AddRoute(route Route) error
    DeleteRoute(key string) error
    UpdateRoute(route Route) error
    ListRoutes() ([]Route, error)
    GetRoutesByVrf(vrfID uint32) ([]Route, error)
    GetRoutesByNextHop(nextHop string) ([]Route, error)
    ValidateRoute(route Route) error
    ImportRoutes(path string) error
    ExportRoutes(path string) error
    // ... and more
}

// Anti-pattern 3: Panic for errors
func badGetRoute(key string) Route {
    r, ok := globalRoutes[key]
    if !ok {
        panic("route not found")  // BAD!
    }
    return r
}

// =====================
// GOOD CODE (DO THIS)
// =====================

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func (r Route) Key() string {
    return fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
}

var ErrNotFound = errors.New("route not found")

// Small interface defined by consumer
type RouteStore interface {
    Get(key string) (Route, error)
    Set(key string, route Route) error
}

// Encapsulated state with mutex
type MemoryStore struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewMemoryStore() *MemoryStore {
    return &MemoryStore{routes: make(map[string]Route)}
}

func (m *MemoryStore) Get(key string) (Route, error) {
    m.mu.RLock()
    defer m.mu.RUnlock()
    
    r, ok := m.routes[key]
    if !ok {
        return Route{}, ErrNotFound
    }
    return r, nil
}

func (m *MemoryStore) Set(key string, route Route) error {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    m.routes[key] = route
    return nil
}

// Simple service with dependency injection
type RouteService struct {
    store  RouteStore
    logger *log.Logger
}

func NewRouteService(store RouteStore) *RouteService {
    return &RouteService{
        store:  store,
        logger: log.Default(),
    }
}

// Return errors, don't panic
func (s *RouteService) GetRoute(vrfID uint32, prefix string) (Route, error) {
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    
    route, err := s.store.Get(key)
    if err != nil {
        if errors.Is(err, ErrNotFound) {
            return Route{}, fmt.Errorf("route %s: %w", key, ErrNotFound)
        }
        return Route{}, fmt.Errorf("get route %s: %w", key, err)
    }
    
    return route, nil
}

func (s *RouteService) AddRoute(route Route) error {
    if route.Prefix == "" {
        return errors.New("prefix required")
    }
    
    return s.store.Set(route.Key(), route)
}

func main() {
    // Explicit initialization, no globals
    store := NewMemoryStore()
    service := NewRouteService(store)
    
    // Add route
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    if err := service.AddRoute(route); err != nil {
        log.Fatalf("add route: %v", err)
    }
    
    // Get route (handle error, don't panic)
    r, err := service.GetRoute(1, "10.0.0.0/24")
    if err != nil {
        log.Fatalf("get route: %v", err)
    }
    fmt.Printf("Found: %+v\n", r)
    
    // Not found is an error, not a panic
    _, err = service.GetRoute(1, "unknown")
    if errors.Is(err, ErrNotFound) {
        fmt.Println("Route not found (handled gracefully)")
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ANTI-PATTERN AVOIDANCE                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   AVOID                              PREFER                             │
│   ─────                              ──────                             │
│                                                                         │
│   Global mutable state         →     Encapsulated state with mutex      │
│   Channels for sync            →     sync.WaitGroup, sync.Mutex         │
│   IXxxService + XxxServiceImpl →     Simple struct + constructor        │
│   interface{} everywhere       →     Generics or specific types         │
│   Multiple close paths         →     defer for cleanup                  │
│   Panic for errors             →     Return error                       │
│   Complex init()               →     Explicit init in main()            │
│   Giant interfaces             →     Small, focused interfaces          │
│   Naked returns                →     Explicit return values             │
│                                                                         │
│   RULES OF THUMB:                                                       │
│   • If you can't test it easily, it's probably wrong                    │
│   • If new team members can't understand it, simplify                   │
│   • If it looks like Java, reconsider                                   │
│   • Simple boring code > clever complex code                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 常见反模式

| 反模式 | 问题 | 解决方案 |
|--------|------|----------|
| 全局可变状态 | 竞态条件、测试困难 | 封装状态 + mutex |
| 过度使用 channel | 复杂、死锁 | 用 WaitGroup/Mutex |
| Java 风格抽象 | 过度设计 | 简单结构体 |
| interface{} 滥用 | 失去类型安全 | 泛型或具体类型 |
| 不用 defer | 资源泄漏 | 总是用 defer |
| 用 panic 处理错误 | 崩溃调用者 | 返回 error |

### 最佳实践

1. 封装状态，不用全局变量
2. Channel 用于通信，sync 用于同步
3. 简单直接，不要过度抽象
4. 用 defer 确保清理
5. 返回错误，不要 panic
6. 保持接口小

### 规则总结

- 如果不易测试，可能有问题
- 如果新人看不懂，需要简化
- 如果看起来像 Java，重新考虑
- 简单无聊的代码 > 聪明复杂的代码
