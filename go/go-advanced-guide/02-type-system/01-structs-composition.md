# Structs and Composition

## 1. Engineering Problem

### What real-world problem does this solve?

**Go rejects class-based inheritance in favor of composition.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    INHERITANCE vs COMPOSITION                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Class Inheritance (C++/Java):                                         │
│   ─────────────────────────────                                         │
│                                                                         │
│           Animal                    Problems:                           │
│              │                      • Deep hierarchies                  │
│      ┌───────┴───────┐              • Fragile base class               │
│      │               │              • Diamond problem                   │
│    Mammal          Bird             • Forced "is-a" relationships       │
│      │               │              • Hard to change later              │
│   ┌──┴──┐        ┌──┴──┐                                               │
│   Dog   Cat    Sparrow Eagle                                            │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Go Composition:                                                       │
│   ───────────────                                                       │
│                                                                         │
│   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐                  │
│   │   Logger    │   │   Metrics   │   │   Config    │                  │
│   └─────────────┘   └─────────────┘   └─────────────┘                  │
│          │                │                 │                           │
│          └────────────────┼─────────────────┘                           │
│                           │                                             │
│                    ┌──────▼──────┐                                      │
│                    │   Server    │  Server HAS-A Logger, Metrics, etc  │
│                    │             │  Not IS-A anything                   │
│                    │  Logger     │                                      │
│                    │  Metrics    │                                      │
│                    │  Config     │                                      │
│                    └─────────────┘                                      │
│                                                                         │
│   Benefits: Flexible, explicit, easy to change                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong with inheritance-style thinking in Go?

1. **Trying to build deep type hierarchies**: Go has no inheritance
2. **Expecting polymorphism from struct embedding**: Embedding is not inheritance
3. **Over-abstracting with interfaces too early**: Leads to Java-style bloat
4. **Ignoring zero values**: Uninitialized fields behave predictably in Go

---

## 2. Core Mental Model

### Structs are data containers

```go
// A struct is a collection of fields
type Route struct {
    VrfID     uint32
    Prefix    string
    NextHop   string
    IsV6      bool
}

// Zero value: all fields are their zero values
var r Route  // {VrfID: 0, Prefix: "", NextHop: "", IsV6: false}
```

### Composition via embedding

```go
// Embedding promotes fields and methods
type VmcRoute struct {
    Route           // Embedded (anonymous field)
    VmcName string  // Additional field
}

// VmcRoute now has VrfID, Prefix, NextHop, IsV6, AND VmcName
vr := VmcRoute{
    Route: Route{
        VrfID:   1,
        Prefix:  "10.0.0.0/24",
        NextHop: "192.168.1.1",
    },
    VmcName: "vmc-01",
}

// Access embedded fields directly
fmt.Println(vr.Prefix)   // "10.0.0.0/24" (promoted)
fmt.Println(vr.VmcName)  // "vmc-01"
```

---

## 3. Language Mechanism

### Struct definition

```go
// Named struct
type RouterAddress struct {
    VrfId     uint32
    Address   string
    PrefixLen uint32
    IsV6      bool
}

// Anonymous struct (inline)
config := struct {
    Host string
    Port int
}{
    Host: "localhost",
    Port: 8080,
}

// Struct tags (metadata for reflection)
type Config struct {
    Host string `json:"host" validate:"required"`
    Port int    `json:"port" validate:"min=1,max=65535"`
}
```

### Field visibility

```go
type Router struct {
    Name    string  // Exported (uppercase) - visible outside package
    version int     // Unexported (lowercase) - package-private
}
```

### Struct comparison

```go
// Structs are comparable if all fields are comparable
r1 := Route{VrfID: 1, Prefix: "10.0.0.0/24"}
r2 := Route{VrfID: 1, Prefix: "10.0.0.0/24"}
fmt.Println(r1 == r2)  // true

// Can use structs as map keys if comparable
routes := make(map[Route]bool)
routes[r1] = true
```

### Embedding mechanics

```go
type Logger struct{}
func (l *Logger) Log(msg string) { fmt.Println(msg) }

type Server struct {
    Logger  // Embedded - Server now has Log method
    Name string
}

s := Server{Name: "main"}
s.Log("starting")  // Calls embedded Logger's Log method

// Explicit access still works
s.Logger.Log("explicit")
```

---

## 4. Idiomatic Usage

### Constructor functions

```go
// Go convention: NewXxx returns *Xxx
func NewRouter(name string, vrfID uint32) *Router {
    return &Router{
        Name:   name,
        VrfID:  vrfID,
        routes: make(map[string]Route),  // Initialize internal state
    }
}

// With validation
func NewRoute(prefix, nextHop string) (*Route, error) {
    if prefix == "" {
        return nil, errors.New("prefix required")
    }
    return &Route{
        Prefix:  prefix,
        NextHop: nextHop,
    }, nil
}
```

### Functional options pattern

```go
type Server struct {
    host    string
    port    int
    timeout time.Duration
}

type ServerOption func(*Server)

func WithPort(port int) ServerOption {
    return func(s *Server) { s.port = port }
}

func WithTimeout(d time.Duration) ServerOption {
    return func(s *Server) { s.timeout = d }
}

func NewServer(host string, opts ...ServerOption) *Server {
    s := &Server{
        host:    host,
        port:    8080,           // default
        timeout: 30 * time.Second,  // default
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}

// Usage
s := NewServer("localhost", WithPort(9090), WithTimeout(time.Minute))
```

### Embedding for behavior reuse

```go
// Reusable component
type Metrics struct {
    requestCount int64
    errorCount   int64
}

func (m *Metrics) RecordRequest() { atomic.AddInt64(&m.requestCount, 1) }
func (m *Metrics) RecordError()   { atomic.AddInt64(&m.errorCount, 1) }

// Compose into server
type Server struct {
    Metrics  // Embedded
    handler  http.Handler
}

// Server now has RecordRequest and RecordError methods
```

---

## 5. Common Pitfalls

### Pitfall 1: Expecting inheritance behavior from embedding

```go
type Base struct{}
func (b *Base) Method() { fmt.Println("Base") }

type Derived struct {
    Base
}
func (d *Derived) Method() { fmt.Println("Derived") }

func callMethod(b *Base) {
    b.Method()  // Always prints "Base"
}

d := &Derived{}
callMethod(&d.Base)  // Prints "Base", not "Derived"!

// Go has no virtual methods - use interfaces for polymorphism
```

### Pitfall 2: Nil pointer in embedded struct

```go
type Client struct {
    *http.Client  // Embedded pointer
}

c := Client{}  // http.Client is nil!
c.Get("...")   // PANIC: nil pointer dereference

// Fix: Initialize the embedded pointer
c := Client{Client: &http.Client{}}
```

### Pitfall 3: Shadowed embedded fields

```go
type Inner struct {
    Name string
}

type Outer struct {
    Inner
    Name string  // Shadows Inner.Name
}

o := Outer{
    Inner: Inner{Name: "inner"},
    Name:  "outer",
}

fmt.Println(o.Name)       // "outer"
fmt.Println(o.Inner.Name) // "inner" (explicit access)
```

### Pitfall 4: Struct copying vs pointer sharing

```go
type Config struct {
    Routes []Route  // Slice header copied, not underlying array
}

c1 := Config{Routes: []Route{{Prefix: "10.0.0.0/24"}}}
c2 := c1  // Shallow copy!

c2.Routes[0].Prefix = "192.168.0.0/16"
fmt.Println(c1.Routes[0].Prefix)  // "192.168.0.0/16" - Modified!

// Fix: Deep copy if needed
c2.Routes = make([]Route, len(c1.Routes))
copy(c2.Routes, c1.Routes)
```

---

## 6. Complete Example

```go
package main

import (
    "encoding/json"
    "fmt"
    "sync"
    "sync/atomic"
    "time"
)

// Base components for composition
type Logger struct {
    prefix string
}

func (l *Logger) Log(format string, args ...interface{}) {
    fmt.Printf("[%s] %s\n", l.prefix, fmt.Sprintf(format, args...))
}

type Metrics struct {
    requests int64
    errors   int64
}

func (m *Metrics) RecordRequest() { atomic.AddInt64(&m.requests, 1) }
func (m *Metrics) RecordError()   { atomic.AddInt64(&m.errors, 1) }
func (m *Metrics) Stats() (requests, errors int64) {
    return atomic.LoadInt64(&m.requests), atomic.LoadInt64(&m.errors)
}

// Domain structs
type Route struct {
    VrfID     uint32 `json:"vrf_id"`
    Prefix    string `json:"prefix"`
    NextHop   string `json:"next_hop"`
    PrefixLen uint32 `json:"prefix_len"`
    IsV6      bool   `json:"is_v6"`
}

type VmcRoute struct {
    Route           // Embedded
    VmcName string  `json:"vmc_name"`
}

// RouteManager composes multiple behaviors
type RouteManager struct {
    Logger   // Embedded for logging
    Metrics  // Embedded for metrics
    
    mu     sync.RWMutex
    routes map[string]VmcRoute
}

// Option pattern for configuration
type RouteManagerOption func(*RouteManager)

func WithLogPrefix(prefix string) RouteManagerOption {
    return func(rm *RouteManager) {
        rm.Logger.prefix = prefix
    }
}

func NewRouteManager(opts ...RouteManagerOption) *RouteManager {
    rm := &RouteManager{
        Logger: Logger{prefix: "RouteManager"},
        routes: make(map[string]VmcRoute),
    }
    for _, opt := range opts {
        opt(rm)
    }
    return rm
}

func (rm *RouteManager) AddRoute(r VmcRoute) {
    rm.RecordRequest()  // From embedded Metrics
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
    
    rm.Log("Added route: %s via %s for VMC %s", r.Prefix, r.NextHop, r.VmcName)
}

func (rm *RouteManager) GetRoute(vrfID uint32, prefix string) (VmcRoute, bool) {
    rm.RecordRequest()
    
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rm.routes[key]
    return r, ok
}

func (rm *RouteManager) DeleteVmcRoutes(vmcName string) int {
    rm.RecordRequest()
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    deleted := 0
    for key, route := range rm.routes {
        if route.VmcName == vmcName {
            delete(rm.routes, key)
            deleted++
            rm.Log("Deleted route: %s for VMC %s", route.Prefix, vmcName)
        }
    }
    return deleted
}

func (rm *RouteManager) ToJSON() ([]byte, error) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    routes := make([]VmcRoute, 0, len(rm.routes))
    for _, r := range rm.routes {
        routes = append(routes, r)
    }
    return json.MarshalIndent(routes, "", "  ")
}

func main() {
    rm := NewRouteManager(WithLogPrefix("RouterMgr"))
    
    // Add some routes
    routes := []VmcRoute{
        {Route: Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1", PrefixLen: 24}, VmcName: "vmc-01"},
        {Route: Route{VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2", PrefixLen: 24}, VmcName: "vmc-01"},
        {Route: Route{VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1", PrefixLen: 16}, VmcName: "vmc-02"},
    }
    
    for _, r := range routes {
        rm.AddRoute(r)
    }
    
    // Query a route
    if r, ok := rm.GetRoute(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", r)
    }
    
    // Delete VMC routes
    deleted := rm.DeleteVmcRoutes("vmc-01")
    fmt.Printf("Deleted %d routes\n", deleted)
    
    // Stats from embedded Metrics
    requests, errors := rm.Stats()
    fmt.Printf("Requests: %d, Errors: %d\n", requests, errors)
    
    // Export as JSON
    data, _ := rm.ToJSON()
    fmt.Printf("Routes:\n%s\n", data)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STRUCT DESIGN RULES                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PREFER COMPOSITION OVER INHERITANCE                                │
│      • Embed structs for behavior reuse                                 │
│      • Use interfaces for polymorphism                                  │
│      • Keep hierarchies flat                                            │
│                                                                         │
│   2. DESIGN FOR ZERO VALUES                                             │
│      • Make zero value useful when possible                             │
│      • Document if zero value is invalid                                │
│      • Use constructor functions for complex init                       │
│                                                                         │
│   3. USE FUNCTIONAL OPTIONS FOR FLEXIBLE CONFIG                         │
│      • Keeps API stable as options grow                                 │
│      • Self-documenting                                                 │
│      • Optional parameters without overloads                            │
│                                                                         │
│   4. EMBEDDING GUIDELINES                                               │
│      • Embed for "has-a" + "behaves-like"                              │
│      • Don't embed just to save typing                                  │
│      • Be aware of field shadowing                                      │
│      • Initialize embedded pointers                                     │
│                                                                         │
│   5. VISIBILITY                                                         │
│      • Export (uppercase) sparingly                                     │
│      • Unexported (lowercase) by default                                │
│      • Keep internal state private                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 结构体与组合核心概念

**Go 使用组合（Composition）而非继承（Inheritance）。**

### 组合 vs 继承

| 特性 | 继承（C++/Java） | 组合（Go） |
|------|-----------------|-----------|
| 关系 | IS-A（是一个） | HAS-A（有一个） |
| 层次 | 深层次结构 | 扁平结构 |
| 耦合 | 紧耦合 | 松耦合 |
| 灵活性 | 难以修改基类 | 容易替换组件 |

### 嵌入（Embedding）

```go
type Server struct {
    Logger   // 嵌入 - Server 现在拥有 Logger 的方法
    Metrics  // 嵌入 - Server 现在拥有 Metrics 的方法
    name string
}
```

**嵌入特性：**
- 字段和方法被"提升"到外层结构
- 可以直接访问 `s.Log()` 而非 `s.Logger.Log()`
- 不是继承！没有多态性

### 函数式选项模式

```go
type Option func(*Server)

func WithPort(p int) Option { return func(s *Server) { s.port = p } }

func NewServer(host string, opts ...Option) *Server {
    s := &Server{host: host, port: 8080}  // 默认值
    for _, opt := range opts {
        opt(s)
    }
    return s
}
```

### 常见错误

1. **期望嵌入实现多态**：Go 没有虚方法
2. **嵌入指针未初始化**：导致 nil 指针 panic
3. **字段遮蔽**：外层字段遮蔽内层同名字段
4. **浅拷贝陷阱**：复制结构体只复制切片头，不复制底层数组

### 设计原则

1. **优先使用组合**
2. **设计有意义的零值**
3. **使用构造函数进行复杂初始化**
4. **使用函数式选项处理可选配置**
5. **默认不导出，按需导出**

