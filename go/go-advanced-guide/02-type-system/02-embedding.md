# Embedded Structs: Promotion, Not Inheritance

## 1. Engineering Problem

### What real-world problem does this solve?

**Embedding provides code reuse without inheritance's complexity.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    EMBEDDING MECHANICS                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   What embedding does:                                                  │
│   ────────────────────                                                  │
│                                                                         │
│   type Inner struct {                type Outer struct {                │
│       Name string                        Inner        // embedded       │
│   }                                      Value int                      │
│                                      }                                  │
│   func (i *Inner) Hello() string {                                      │
│       return "Hello, " + i.Name      o := Outer{Inner: Inner{Name: "X"}}│
│   }                                  o.Hello()     // Works!            │
│                                      o.Name        // Works!            │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   What embedding is NOT:                                                │
│   ──────────────────────                                                │
│                                                                         │
│   • NOT inheritance (no polymorphism with outer type)                   │
│   • NOT subtyping (Outer is not an Inner)                              │
│   • NOT overriding (outer methods shadow, don't override)               │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Memory layout:                                                        │
│                                                                         │
│   Outer                                                                 │
│   ┌─────────────────────────┐                                           │
│   │ Inner ┌───────────────┐ │  Inner is literally inside Outer         │
│   │       │ Name: "X"     │ │  Not a pointer (unless embedded as *T)   │
│   │       └───────────────┘ │                                           │
│   │ Value: 42               │                                           │
│   └─────────────────────────┘                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Promotion rules

```go
type Logger struct{}
func (l Logger) Log(s string) { fmt.Println(s) }
func (l Logger) Prefix() string { return "[LOG]" }

type Server struct {
    Logger  // Embedded
    Name    string
}

s := Server{Name: "main"}

// Promoted methods - called on outer type
s.Log("hello")      // Same as s.Logger.Log("hello")

// Promoted fields (if Logger had any)
// s.SomeField     // Same as s.Logger.SomeField
```

### Method resolution order

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    METHOD RESOLUTION                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   When you call outer.Method():                                         │
│                                                                         │
│   1. Check Outer's own methods first                                    │
│   2. Check embedded types (one level deep)                              │
│   3. If multiple embedded types have Method → compile error             │
│                                                                         │
│   type A struct{}                                                       │
│   func (A) Do() { println("A") }                                        │
│                                                                         │
│   type B struct{}                                                       │
│   func (B) Do() { println("B") }                                        │
│                                                                         │
│   type C struct {                                                       │
│       A                                                                 │
│       B                                                                 │
│   }                                                                     │
│                                                                         │
│   c := C{}                                                              │
│   c.Do()      // ERROR: ambiguous selector c.Do                         │
│   c.A.Do()    // OK: "A"                                                │
│   c.B.Do()    // OK: "B"                                                │
│                                                                         │
│   type D struct {                                                       │
│       A                                                                 │
│       B                                                                 │
│   }                                                                     │
│   func (D) Do() { println("D") }  // D's own method wins                │
│                                                                         │
│   d := D{}                                                              │
│   d.Do()      // "D" - outer type's method takes precedence             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Embedding types

```go
// Embed struct value
type Server struct {
    Logger           // Logger value embedded
}

// Embed struct pointer
type Client struct {
    *http.Client     // *http.Client embedded
}

// Embed interface
type Handler struct {
    http.Handler     // Interface embedded
}
```

### Interface satisfaction via embedding

```go
type Reader interface {
    Read([]byte) (int, error)
}

type MyReader struct{}
func (MyReader) Read(p []byte) (int, error) { return 0, nil }

type BufferedReader struct {
    MyReader  // Embeds MyReader
    buffer    []byte
}

// BufferedReader satisfies Reader via embedded MyReader
var r Reader = BufferedReader{}  // OK!
```

### Embedding and JSON

```go
type Timestamp struct {
    CreatedAt time.Time `json:"created_at"`
    UpdatedAt time.Time `json:"updated_at"`
}

type Route struct {
    Timestamp        `json:",inline"`  // Flattens into Route's JSON
    VrfID     uint32 `json:"vrf_id"`
    Prefix    string `json:"prefix"`
}

// JSON output:
// {"created_at":"...","updated_at":"...","vrf_id":1,"prefix":"10.0.0.0/24"}
```

---

## 4. Idiomatic Usage

### Pattern 1: Behavior composition

```go
// Reusable behaviors
type Locker struct {
    sync.Mutex
}

type Counter struct {
    sync.Mutex
    count int64
}

func (c *Counter) Inc() {
    c.Lock()         // Promoted from sync.Mutex
    defer c.Unlock()
    c.count++
}
```

### Pattern 2: Extending stdlib types

```go
type MyHandler struct {
    http.Handler  // Embed interface
}

// Wrap with additional behavior
func (h MyHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    log.Printf("Request: %s %s", r.Method, r.URL.Path)
    h.Handler.ServeHTTP(w, r)  // Delegate to embedded handler
}
```

### Pattern 3: Embedding for mutex protection

```go
// Common pattern: embed mutex with protected data
type SafeMap struct {
    mu sync.RWMutex
    m  map[string]int
}

// Better: embed mutex directly
type SafeCounter struct {
    sync.Mutex  // Embedded
    value int
}

func (c *SafeCounter) Inc() {
    c.Lock()
    c.value++
    c.Unlock()
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Expecting polymorphic behavior

```go
type Base struct{}
func (b Base) Who() string { return "Base" }
func (b Base) Greet() string { return "Hello, " + b.Who() }

type Derived struct {
    Base
}
func (d Derived) Who() string { return "Derived" }

d := Derived{}
fmt.Println(d.Greet())  // "Hello, Base" NOT "Hello, Derived"!

// Why? Base.Greet() calls Base.Who(), not Derived.Who()
// Go has no virtual dispatch

// Fix: Use interface if you need polymorphism
type Identifier interface {
    Who() string
}

func Greet(i Identifier) string {
    return "Hello, " + i.Who()
}
```

### Pitfall 2: Nil embedded pointer

```go
type Wrapper struct {
    *Inner  // Pointer embedding
}

w := Wrapper{}  // Inner is nil!
w.SomeMethod()  // PANIC: nil pointer

// Always check or initialize
if w.Inner != nil {
    w.SomeMethod()
}

// Or use value embedding
type SafeWrapper struct {
    Inner  // Value embedding - always initialized
}
```

### Pitfall 3: Forgetting explicit access when shadowed

```go
type Conn struct {
    Timeout time.Duration
}

type Client struct {
    Conn
    Timeout time.Duration  // Shadows Conn.Timeout
}

c := Client{
    Conn:    Conn{Timeout: time.Second},
    Timeout: time.Minute,
}

fmt.Println(c.Timeout)       // time.Minute (outer)
fmt.Println(c.Conn.Timeout)  // time.Second (must be explicit)
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
    "time"
)

// Reusable components via embedding

// LifecycleHooks provides start/stop tracking
type LifecycleHooks struct {
    started   atomic.Bool
    startTime time.Time
    stopTime  time.Time
}

func (l *LifecycleHooks) MarkStarted() {
    l.started.Store(true)
    l.startTime = time.Now()
}

func (l *LifecycleHooks) MarkStopped() {
    l.started.Store(false)
    l.stopTime = time.Now()
}

func (l *LifecycleHooks) IsRunning() bool {
    return l.started.Load()
}

func (l *LifecycleHooks) Uptime() time.Duration {
    if !l.IsRunning() {
        return 0
    }
    return time.Since(l.startTime)
}

// RequestMetrics tracks request statistics
type RequestMetrics struct {
    total   atomic.Int64
    failed  atomic.Int64
    latency atomic.Int64  // nanoseconds, for average calculation
}

func (m *RequestMetrics) Record(duration time.Duration, success bool) {
    m.total.Add(1)
    m.latency.Add(int64(duration))
    if !success {
        m.failed.Add(1)
    }
}

func (m *RequestMetrics) Stats() (total, failed int64, avgLatency time.Duration) {
    total = m.total.Load()
    failed = m.failed.Load()
    if total > 0 {
        avgLatency = time.Duration(m.latency.Load() / total)
    }
    return
}

// RouteManager composes multiple behaviors
type RouteManager struct {
    LifecycleHooks  // Embedded - gains start/stop tracking
    RequestMetrics  // Embedded - gains request metrics
    
    mu     sync.RWMutex
    routes map[string]Route
    name   string
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteManager(name string) *RouteManager {
    return &RouteManager{
        routes: make(map[string]Route),
        name:   name,
    }
}

func (rm *RouteManager) Start() {
    rm.MarkStarted()  // From LifecycleHooks
    fmt.Printf("[%s] Started\n", rm.name)
}

func (rm *RouteManager) Stop() {
    rm.MarkStopped()  // From LifecycleHooks
    fmt.Printf("[%s] Stopped (uptime: %v)\n", rm.name, rm.Uptime())
}

func (rm *RouteManager) AddRoute(r Route) error {
    start := time.Now()
    defer func() {
        rm.Record(time.Since(start), true)  // From RequestMetrics
    }()
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
    return nil
}

func (rm *RouteManager) GetRoute(vrfID uint32, prefix string) (Route, bool) {
    start := time.Now()
    defer func() {
        rm.Record(time.Since(start), true)
    }()
    
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rm.routes[key]
    return r, ok
}

func (rm *RouteManager) Status() string {
    total, failed, avgLatency := rm.Stats()  // From RequestMetrics
    return fmt.Sprintf(
        "name=%s running=%v uptime=%v requests=%d failed=%d avg_latency=%v",
        rm.name, rm.IsRunning(), rm.Uptime(), total, failed, avgLatency,
    )
}

func main() {
    rm := NewRouteManager("router-mgr")
    rm.Start()
    
    // Add routes
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
        {VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1"},
    }
    
    for _, r := range routes {
        rm.AddRoute(r)
    }
    
    // Query
    if r, ok := rm.GetRoute(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", r)
    }
    
    // Status uses both embedded types
    fmt.Println(rm.Status())
    
    time.Sleep(100 * time.Millisecond)
    rm.Stop()
    fmt.Println(rm.Status())
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    EMBEDDING DESIGN RULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   DO:                                                                   │
│   • Embed for "has-a AND behaves-like" relationships                   │
│   • Embed sync.Mutex when struct needs internal locking                 │
│   • Embed interfaces for decoration/wrapping patterns                   │
│   • Use embedding to satisfy interfaces                                 │
│                                                                         │
│   DON'T:                                                                │
│   • Embed just to avoid typing field names                              │
│   • Expect inheritance/polymorphism behavior                            │
│   • Embed pointers without initializing them                            │
│   • Create deep embedding hierarchies                                   │
│                                                                         │
│   REMEMBER:                                                             │
│   • Embedding is composition, not inheritance                           │
│   • No virtual dispatch - base methods call base methods                │
│   • Outer methods shadow, don't override                                │
│   • Interface satisfaction IS polymorphic                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 嵌入核心概念

**嵌入是组合的一种形式，不是继承。**

### 方法提升

当你嵌入一个类型时：
- 嵌入类型的字段和方法被"提升"到外层类型
- 可以直接在外层类型上调用
- 编译器自动转发调用

### 关键区别：嵌入 vs 继承

| 特性 | 继承 | 嵌入 |
|------|------|------|
| 多态 | 支持虚方法 | 不支持 |
| 类型关系 | 子类型关系 | 无子类型关系 |
| 方法覆盖 | 真正的覆盖 | 遮蔽（shadow） |
| 基类方法调用自身 | 可能调用派生类方法 | 总是调用自己的方法 |

### 常见陷阱

1. **期望多态行为**：嵌入不提供虚方法调度
2. **嵌入指针未初始化**：导致 nil 指针 panic
3. **字段遮蔽**：外层同名字段遮蔽内层字段
4. **多重嵌入冲突**：多个嵌入类型有同名方法会编译错误

### 最佳实践

1. 用于"拥有且行为像"的关系
2. 嵌入 sync.Mutex 进行内部锁保护
3. 嵌入接口实现装饰器模式
4. 保持嵌入层次扁平

