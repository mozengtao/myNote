# context.Context: Cancellation and Request Scoping

## 1. Engineering Problem

### What real-world problem does this solve?

**How do you propagate cancellation, deadlines, and request-scoped data through a call chain?**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE CANCELLATION PROBLEM                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   HTTP Request arrives                                                  │
│        │                                                                │
│        ▼                                                                │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐             │
│   │ Handler │───►│ Service │───►│  Repo   │───►│   DB    │             │
│   └─────────┘    └─────────┘    └─────────┘    └─────────┘             │
│                                                                         │
│   Client disconnects at Handler level...                                │
│   How does DB query know to stop?                                       │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Without context:                                                      │
│   • Manual cancellation flags through every layer                       │
│   • Easy to forget one layer                                            │
│   • No standard pattern                                                 │
│   • Deadlines managed separately from cancellation                      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   With context:                                                         │
│                                                                         │
│   ctx ─────────────────────────────────────────────────────────────►    │
│    │                                                                    │
│    ├── Handler checks ctx.Done()                                        │
│    │        │                                                           │
│    │        └── Service checks ctx.Done()                               │
│    │                    │                                               │
│    │                    └── Repo checks ctx.Done()                      │
│    │                              │                                     │
│    │                              └── DB query uses ctx                 │
│    │                                                                    │
│    └── Parent cancels → ALL children notified                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong without context?

1. **Wasted resources**: Requests complete even after client disconnects
2. **Resource exhaustion**: Slow operations pile up
3. **No timeout propagation**: Each layer manages timeouts independently
4. **Inconsistent cancellation**: Some operations cancel, others don't
5. **Difficult testing**: Can't simulate timeout scenarios

---

## 2. Core Mental Model

### Context is a request-scoped tree

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      CONTEXT TREE MODEL                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   context.Background() ◄── Root (never cancels)                         │
│           │                                                             │
│           ▼                                                             │
│   context.WithCancel()                                                  │
│           │                                                             │
│           ├─────────────────────────────────────────────┐               │
│           ▼                                             ▼               │
│   context.WithTimeout()                     context.WithValue()         │
│           │                                             │               │
│           ├─────────────┐                               │               │
│           ▼             ▼                               ▼               │
│        child1        child2                          child3             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Key properties:                                                       │
│                                                                         │
│   1. Cancellation flows DOWN the tree (parent → children)               │
│   2. Values flow DOWN the tree (parent values visible to children)      │
│   3. Deadlines are inherited (child can have shorter, not longer)       │
│   4. Context is immutable (With* returns new context)                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### The context.Context interface

```go
type Context interface {
    // Deadline returns the time when work should be cancelled
    Deadline() (deadline time.Time, ok bool)
    
    // Done returns a channel closed when work should be cancelled
    Done() <-chan struct{}
    
    // Err returns nil if Done is not closed, otherwise:
    // - Canceled if the context was cancelled
    // - DeadlineExceeded if the deadline passed
    Err() error
    
    // Value returns value for key, or nil if not found
    Value(key interface{}) interface{}
}
```

---

## 3. Language Mechanism

### Creating contexts

```go
// Root contexts (usually only in main or tests)
ctx := context.Background()  // Never cancels
ctx := context.TODO()        // Placeholder (same as Background, but signals intent)

// Derived contexts
ctx, cancel := context.WithCancel(parent)
defer cancel()  // ALWAYS defer cancel to release resources

ctx, cancel := context.WithTimeout(parent, 5*time.Second)
defer cancel()

ctx, cancel := context.WithDeadline(parent, time.Now().Add(5*time.Second))
defer cancel()

ctx := context.WithValue(parent, key, value)  // No cancel needed
```

### Checking cancellation

```go
// In a loop
for {
    select {
    case <-ctx.Done():
        return ctx.Err()  // Returns Canceled or DeadlineExceeded
    default:
        doWork()
    }
}

// Before expensive operation
if ctx.Err() != nil {
    return ctx.Err()
}

// Passing to blocking calls
result, err := db.QueryContext(ctx, query)  // Respects cancellation
resp, err := http.NewRequestWithContext(ctx, "GET", url, nil)
```

### Context values

```go
// Define custom key type to avoid collisions
type contextKey string

const userIDKey contextKey = "userID"

// Set value
ctx := context.WithValue(parent, userIDKey, "user-123")

// Get value
if userID, ok := ctx.Value(userIDKey).(string); ok {
    // Use userID
}
```

---

## 4. Idiomatic Usage

### Rule 1: Context is the first parameter

```go
// CORRECT: Context first
func GetUser(ctx context.Context, id string) (*User, error)
func (s *Server) HandleRequest(ctx context.Context, req *Request) error

// WRONG: Context not first
func GetUser(id string, ctx context.Context) (*User, error)
```

### Rule 2: Always defer cancel

```go
// CORRECT: Resources released even on panic
ctx, cancel := context.WithTimeout(parent, time.Second)
defer cancel()

// WRONG: Leak if function returns early or panics
ctx, cancel := context.WithTimeout(parent, time.Second)
// ... code ...
cancel()  // Might not be reached
```

### Rule 3: Don't store context in structs

```go
// WRONG: Context stored in struct
type Server struct {
    ctx context.Context  // Don't do this
}

// CORRECT: Pass context per-request
func (s *Server) Handle(ctx context.Context, req Request) error
```

### Rule 4: Use values sparingly

```go
// WRONG: Using context for everything
ctx = context.WithValue(ctx, "db", db)
ctx = context.WithValue(ctx, "logger", logger)
ctx = context.WithValue(ctx, "config", config)

// CORRECT: Pass explicit dependencies, use values only for request-scoped data
func Handle(ctx context.Context, db *DB, logger *Logger, req Request) error {
    // ctx values: request ID, user ID, trace ID
    requestID := ctx.Value(requestIDKey).(string)
}
```

### Appropriate context values

```go
// Good uses of context values:
// - Request IDs
// - Trace/span IDs
// - Authentication info
// - Request-scoped deadlines

// Bad uses:
// - Database connections
// - Loggers
// - Configuration
// - Anything that's not request-scoped
```

---

## 5. Common Pitfalls

### Pitfall 1: Forgetting to cancel

```go
// LEAK: cancel never called, resources not freed
func bad() {
    ctx, cancel := context.WithTimeout(context.Background(), time.Hour)
    _ = cancel  // Silence "unused" warning, but still wrong
    doWork(ctx)
}

// CORRECT
func good() {
    ctx, cancel := context.WithTimeout(context.Background(), time.Hour)
    defer cancel()
    doWork(ctx)
}
```

### Pitfall 2: Using wrong parent context

```go
// WRONG: Background loses request cancellation
func handler(reqCtx context.Context) {
    ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
    // If reqCtx is cancelled, this won't notice
    defer cancel()
    doWork(ctx)
}

// CORRECT: Derive from request context
func handler(reqCtx context.Context) {
    ctx, cancel := context.WithTimeout(reqCtx, time.Minute)
    defer cancel()
    doWork(ctx)  // Cancels if reqCtx cancels OR timeout
}
```

### Pitfall 3: Ignoring context in loops

```go
// WRONG: Loop doesn't check context
func processMany(ctx context.Context, items []Item) error {
    for _, item := range items {
        process(item)  // ctx ignored!
    }
    return nil
}

// CORRECT: Check context in loop
func processMany(ctx context.Context, items []Item) error {
    for _, item := range items {
        select {
        case <-ctx.Done():
            return ctx.Err()
        default:
        }
        if err := process(ctx, item); err != nil {
            return err
        }
    }
    return nil
}
```

### Pitfall 4: Context value key collisions

```go
// WRONG: String key can collide with other packages
ctx = context.WithValue(ctx, "userID", id)

// CORRECT: Package-private type key
type contextKey struct{ name string }
var userIDKey = contextKey{"userID"}
ctx = context.WithValue(ctx, userIDKey, id)
```

### Pitfall 5: Blocking without context

```go
// WRONG: Blocks forever if server hangs
resp, err := http.Get(url)

// CORRECT: Respects cancellation
req, _ := http.NewRequestWithContext(ctx, "GET", url, nil)
resp, err := client.Do(req)
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "math/rand"
    "sync"
    "time"
)

// Request-scoped context keys
type contextKey string

const (
    requestIDKey contextKey = "requestID"
    userIDKey    contextKey = "userID"
)

// RouteService demonstrates proper context usage
type RouteService struct {
    db     *MockDB
    cache  *MockCache
    logger *Logger
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteService() *RouteService {
    return &RouteService{
        db:     &MockDB{},
        cache:  &MockCache{},
        logger: &Logger{},
    }
}

// GetRoute shows context propagation through layers
func (s *RouteService) GetRoute(ctx context.Context, vrfID uint32, prefix string) (*Route, error) {
    // Log with request context
    s.logger.Info(ctx, "GetRoute called", "vrfID", vrfID, "prefix", prefix)
    
    // Check context before expensive operations
    if err := ctx.Err(); err != nil {
        return nil, fmt.Errorf("cancelled before start: %w", err)
    }
    
    // Try cache first (with derived context)
    cacheCtx, cacheCancel := context.WithTimeout(ctx, 100*time.Millisecond)
    defer cacheCancel()
    
    if route, err := s.cache.Get(cacheCtx, vrfID, prefix); err == nil {
        s.logger.Info(ctx, "cache hit")
        return route, nil
    }
    
    // Check if parent context cancelled during cache lookup
    if err := ctx.Err(); err != nil {
        return nil, err
    }
    
    // Query database (uses parent context timeout)
    route, err := s.db.GetRoute(ctx, vrfID, prefix)
    if err != nil {
        return nil, fmt.Errorf("db query failed: %w", err)
    }
    
    // Populate cache (fire-and-forget with short timeout)
    go func() {
        cacheWriteCtx, cancel := context.WithTimeout(context.Background(), time.Second)
        defer cancel()
        _ = s.cache.Set(cacheWriteCtx, route)
    }()
    
    return route, nil
}

// UpdateRoutes shows context in batch operations
func (s *RouteService) UpdateRoutes(ctx context.Context, routes []Route) error {
    s.logger.Info(ctx, "UpdateRoutes called", "count", len(routes))
    
    // Process with bounded concurrency
    const maxConcurrent = 5
    sem := make(chan struct{}, maxConcurrent)
    
    var wg sync.WaitGroup
    errCh := make(chan error, len(routes))
    
    for _, route := range routes {
        // Check cancellation before starting new work
        select {
        case <-ctx.Done():
            // Wait for in-flight operations
            wg.Wait()
            return ctx.Err()
        case sem <- struct{}{}:
            // Got semaphore, proceed
        }
        
        wg.Add(1)
        go func(r Route) {
            defer wg.Done()
            defer func() { <-sem }()
            
            // Per-route timeout derived from parent
            routeCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
            defer cancel()
            
            if err := s.db.UpdateRoute(routeCtx, r); err != nil {
                errCh <- fmt.Errorf("update %s: %w", r.Prefix, err)
            }
        }(route)
    }
    
    wg.Wait()
    close(errCh)
    
    // Collect errors
    var errs []error
    for err := range errCh {
        errs = append(errs, err)
    }
    
    if len(errs) > 0 {
        return fmt.Errorf("update failed: %d errors", len(errs))
    }
    
    return nil
}

// WatchRoutes demonstrates long-running context-aware operation
func (s *RouteService) WatchRoutes(ctx context.Context, callback func(Route)) error {
    s.logger.Info(ctx, "WatchRoutes started")
    defer s.logger.Info(ctx, "WatchRoutes stopped")
    
    ticker := time.NewTicker(time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        case <-ticker.C:
            // Simulate watching for changes
            if route := s.pollChanges(ctx); route != nil {
                callback(*route)
            }
        }
    }
}

func (s *RouteService) pollChanges(ctx context.Context) *Route {
    select {
    case <-ctx.Done():
        return nil
    default:
        // Simulate occasional route changes
        if rand.Float32() < 0.3 {
            return &Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
        }
        return nil
    }
}

// Mock implementations
type MockDB struct{}

func (db *MockDB) GetRoute(ctx context.Context, vrfID uint32, prefix string) (*Route, error) {
    select {
    case <-ctx.Done():
        return nil, ctx.Err()
    case <-time.After(50 * time.Millisecond):
        return &Route{VrfID: vrfID, Prefix: prefix, NextHop: "10.0.0.1"}, nil
    }
}

func (db *MockDB) UpdateRoute(ctx context.Context, r Route) error {
    select {
    case <-ctx.Done():
        return ctx.Err()
    case <-time.After(30 * time.Millisecond):
        return nil
    }
}

type MockCache struct{}

func (c *MockCache) Get(ctx context.Context, vrfID uint32, prefix string) (*Route, error) {
    select {
    case <-ctx.Done():
        return nil, ctx.Err()
    default:
        return nil, errors.New("cache miss")
    }
}

func (c *MockCache) Set(ctx context.Context, r *Route) error {
    select {
    case <-ctx.Done():
        return ctx.Err()
    default:
        return nil
    }
}

type Logger struct{}

func (l *Logger) Info(ctx context.Context, msg string, kv ...interface{}) {
    requestID, _ := ctx.Value(requestIDKey).(string)
    if requestID == "" {
        requestID = "unknown"
    }
    log.Printf("[%s] %s %v", requestID, msg, kv)
}

// Middleware demonstrates context enrichment
func WithRequestID(ctx context.Context) context.Context {
    id := fmt.Sprintf("req-%d", time.Now().UnixNano())
    return context.WithValue(ctx, requestIDKey, id)
}

func main() {
    service := NewRouteService()
    
    // Simulate a request with timeout
    ctx := WithRequestID(context.Background())
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()
    
    // Get a route
    route, err := service.GetRoute(ctx, 1, "10.0.0.0/24")
    if err != nil {
        log.Printf("GetRoute failed: %v", err)
    } else {
        log.Printf("Got route: %+v", route)
    }
    
    // Batch update
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "10.0.0.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "10.0.0.2"},
        {VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.3"},
    }
    
    if err := service.UpdateRoutes(ctx, routes); err != nil {
        log.Printf("UpdateRoutes failed: %v", err)
    }
    
    // Watch with cancellation
    watchCtx, watchCancel := context.WithTimeout(ctx, 3*time.Second)
    defer watchCancel()
    
    err = service.WatchRoutes(watchCtx, func(r Route) {
        log.Printf("Route changed: %+v", r)
    })
    if errors.Is(err, context.DeadlineExceeded) {
        log.Println("Watch ended due to timeout")
    }
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 7. Design Takeaways

### Context Rules

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CONTEXT RULES                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. Context is ALWAYS the first parameter                              │
│      func DoThing(ctx context.Context, ...)                             │
│                                                                         │
│   2. ALWAYS defer cancel()                                              │
│      ctx, cancel := context.WithTimeout(parent, d)                      │
│      defer cancel()                                                     │
│                                                                         │
│   3. NEVER store context in a struct                                    │
│      Pass it through function calls                                     │
│                                                                         │
│   4. Derive from request context, not Background                        │
│      Use the context from the incoming request                          │
│                                                                         │
│   5. Use values ONLY for request-scoped data                            │
│      Not for dependencies like DB, logger, config                       │
│                                                                         │
│   6. Check ctx.Done() in loops and before expensive ops                 │
│      Allow early termination                                            │
│                                                                         │
│   7. All blocking operations should respect context                     │
│      Use XxxContext variants of standard library                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Context 核心概念

**context.Context 是 Go 中用于在 API 边界和进程之间传递取消信号、超时和请求范围值的标准方式。**

### Context 树模型

- `context.Background()` 是根节点，永不取消
- 使用 `With*` 函数创建子 context
- 取消从父节点向下传播到所有子节点
- 值也从父节点向下传播
- Context 是不可变的（With* 返回新 context）

### 关键规则

| 规则 | 说明 |
|------|------|
| Context 必须是第一个参数 | `func DoThing(ctx context.Context, ...)` |
| 总是 defer cancel() | 防止资源泄漏 |
| 不要在 struct 中存储 context | 每次请求传递 |
| 从请求 context 派生 | 不要用 Background |
| 值仅用于请求范围数据 | 不用于依赖注入 |
| 在循环中检查 ctx.Done() | 允许提前终止 |

### Context 值的正确使用

**适合放入 context 值的：**
- 请求 ID
- 追踪 ID / Span ID
- 认证信息
- 请求级别的截止时间

**不适合放入 context 值的：**
- 数据库连接
- Logger
- 配置
- 任何非请求范围的东西

### 常见错误

1. **忘记调用 cancel**：导致资源泄漏
2. **使用错误的父 context**：丢失请求级别的取消信号
3. **在循环中忽略 context**：无法提前终止
4. **使用字符串作为值的 key**：可能与其他包冲突
5. **阻塞操作不尊重 context**：无法取消

