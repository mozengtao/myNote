# panic vs error: When to Use Each

## 1. Engineering Problem

### What real-world problem does this solve?

**Choosing between panic and error determines program robustness.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PANIC vs ERROR                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ERROR (return value):               PANIC (exceptional):              │
│   ────────────────────                ───────────────────               │
│                                                                         │
│   • Expected failures                 • Programming bugs                │
│   • Caller can handle                 • Unrecoverable state             │
│   • Part of API contract              • Violated invariants             │
│   • Explicit control flow             • Crashes goroutine               │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Use ERROR for:                      Use PANIC for:                    │
│   ───────────────                     ──────────────                    │
│   • File not found                    • Nil pointer dereference         │
│   • Network timeout                   • Index out of bounds             │
│   • Invalid user input                • Type assertion failure          │
│   • Resource busy                     • Impossible code paths           │
│   • Permission denied                 • Failed invariant checks         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Rule of thumb:                                                        │
│   • If caller might want to handle it → error                          │
│   • If it's a bug in the program → panic                               │
│   • If unsure → error (safer)                                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Error: Expected failure path

```go
func GetRoute(key string) (Route, error) {
    route, ok := cache[key]
    if !ok {
        return Route{}, ErrNotFound  // Expected: caller handles
    }
    return route, nil
}

// Caller decides what to do
route, err := GetRoute(key)
if errors.Is(err, ErrNotFound) {
    route = createDefaultRoute()  // Handle gracefully
}
```

### Panic: Bug in program

```go
func (rm *RouteManager) mustGetVrf(id uint32) *Vrf {
    vrf := rm.vrfs[id]
    if vrf == nil {
        panic(fmt.Sprintf("VRF %d not initialized", id))  // Bug!
    }
    return vrf
}

// This should never fail if program is correct
// Panic forces fix, not workaround
```

---

## 3. Language Mechanism

### Runtime panics

```go
// These panic automatically:

var p *int
*p = 1  // nil pointer dereference

s := []int{1, 2, 3}
_ = s[10]  // index out of range

var i interface{} = "string"
_ = i.(int)  // type assertion failure (without ok)

m := map[string]int(nil)
m["key"] = 1  // assignment to nil map
```

### Explicit panics

```go
// For bugs and invariant violations
func NewServer(config *Config) *Server {
    if config == nil {
        panic("config cannot be nil")  // Programming error
    }
    return &Server{config: config}
}

// For impossible code paths
switch v := value.(type) {
case int:
    // ...
case string:
    // ...
default:
    panic(fmt.Sprintf("unexpected type: %T", v))
}
```

### Must* pattern

```go
// Error-returning version
func ParseRoute(s string) (Route, error) {
    // ... parsing that can fail
}

// Panic version for initialization
func MustParseRoute(s string) Route {
    r, err := ParseRoute(s)
    if err != nil {
        panic(fmt.Sprintf("invalid route %q: %v", s, err))
    }
    return r
}

// Usage in init or var blocks
var defaultRoute = MustParseRoute("10.0.0.0/24")
```

---

## 4. Idiomatic Usage

### Package initialization

```go
var (
    // OK to panic during init - program can't run anyway
    config = mustLoadConfig()
    db     = mustConnectDB()
)

func mustLoadConfig() *Config {
    c, err := loadConfig()
    if err != nil {
        panic("failed to load config: " + err.Error())
    }
    return c
}
```

### Invariant checks

```go
func (rm *RouteManager) addRoute(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    // Invariant: routes map is always initialized
    if rm.routes == nil {
        panic("routes map not initialized")  // Bug in constructor
    }
    
    rm.routes[r.Key()] = r
}
```

### Development assertions

```go
func assert(condition bool, msg string) {
    if !condition {
        panic("assertion failed: " + msg)
    }
}

func processRoute(r Route) {
    assert(r.Prefix != "", "route must have prefix")
    // ... rest of function
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Panic for expected errors

```go
// BAD: Panics for normal condition
func GetUser(id string) *User {
    user, ok := users[id]
    if !ok {
        panic("user not found")  // This is normal!
    }
    return user
}

// GOOD: Return error
func GetUser(id string) (*User, error) {
    user, ok := users[id]
    if !ok {
        return nil, ErrNotFound
    }
    return user, nil
}
```

### Pitfall 2: Error for bugs

```go
// BAD: Returns error for programming bug
func (s *Server) Start() error {
    if s.config == nil {
        return errors.New("config is nil")  // This is a bug!
    }
    // ...
}

// GOOD: Panic for bugs
func (s *Server) Start() error {
    if s.config == nil {
        panic("Server.Start called with nil config")  // Bug!
    }
    // ...
}
```

### Pitfall 3: Panic crossing API boundary

```go
// BAD: Library panics, kills caller's program
func (lib *Library) Process(data []byte) Result {
    if len(data) == 0 {
        panic("empty data")  // Crashes caller!
    }
    // ...
}

// GOOD: Return error, let caller decide
func (lib *Library) Process(data []byte) (Result, error) {
    if len(data) == 0 {
        return Result{}, ErrEmptyData
    }
    // ...
}
```

---

## 6. Complete Example

```go
package main

import (
    "errors"
    "fmt"
    "log"
)

var ErrNotFound = errors.New("not found")

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type RouteManager struct {
    routes map[string]Route
    vrfs   map[uint32]bool
}

// NewRouteManager creates a new manager.
// Panics if vrfIDs is empty (programming error).
func NewRouteManager(vrfIDs []uint32) *RouteManager {
    if len(vrfIDs) == 0 {
        panic("NewRouteManager: at least one VRF required")
    }
    
    vrfs := make(map[uint32]bool)
    for _, id := range vrfIDs {
        vrfs[id] = true
    }
    
    return &RouteManager{
        routes: make(map[string]Route),
        vrfs:   vrfs,
    }
}

// GetRoute returns error for expected conditions
func (rm *RouteManager) GetRoute(vrfID uint32, prefix string) (Route, error) {
    // Check VRF exists (expected to sometimes fail)
    if !rm.vrfs[vrfID] {
        return Route{}, fmt.Errorf("VRF %d: %w", vrfID, ErrNotFound)
    }
    
    // Check route exists (expected to sometimes fail)
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    route, ok := rm.routes[key]
    if !ok {
        return Route{}, fmt.Errorf("route %s: %w", prefix, ErrNotFound)
    }
    
    return route, nil
}

// addRouteInternal has invariant: VRF must exist
func (rm *RouteManager) addRouteInternal(r Route) {
    // Invariant check - panic if violated
    if !rm.vrfs[r.VrfID] {
        panic(fmt.Sprintf("addRouteInternal: VRF %d not configured", r.VrfID))
    }
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
}

// AddRoute validates and adds a route
func (rm *RouteManager) AddRoute(r Route) error {
    // Validation (expected failures)
    if r.Prefix == "" {
        return errors.New("prefix required")
    }
    if !rm.vrfs[r.VrfID] {
        return fmt.Errorf("VRF %d not configured", r.VrfID)
    }
    
    // Internal function can assume VRF exists
    rm.addRouteInternal(r)
    return nil
}

// MustAddRoute panics on error (for initialization)
func (rm *RouteManager) MustAddRoute(r Route) {
    if err := rm.AddRoute(r); err != nil {
        panic(fmt.Sprintf("MustAddRoute: %v", err))
    }
}

// MustParseRoute is for static initialization
func MustParseRoute(vrfID uint32, prefix, nextHop string) Route {
    if prefix == "" {
        panic("MustParseRoute: empty prefix")
    }
    return Route{VrfID: vrfID, Prefix: prefix, NextHop: nextHop}
}

func main() {
    // Panic OK during initialization
    rm := NewRouteManager([]uint32{1, 2})
    
    // Must pattern for init
    rm.MustAddRoute(MustParseRoute(1, "10.0.0.0/24", "192.168.1.1"))
    
    // Normal operation uses errors
    route, err := rm.GetRoute(1, "10.0.0.0/24")
    if err != nil {
        log.Printf("Error: %v", err)
    } else {
        fmt.Printf("Found: %+v\n", route)
    }
    
    // Expected failure
    _, err = rm.GetRoute(1, "unknown")
    if errors.Is(err, ErrNotFound) {
        log.Println("Route not found (expected)")
    }
    
    // This would panic (programming error)
    // rm.MustAddRoute(Route{VrfID: 99, Prefix: "x"})
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PANIC vs ERROR RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   USE ERROR:                                                            │
│   • File/network operations                                             │
│   • User input validation                                               │
│   • Resource not found                                                  │
│   • Expected runtime conditions                                         │
│   • Anything caller might handle                                        │
│                                                                         │
│   USE PANIC:                                                            │
│   • Nil pointer that shouldn't be nil                                  │
│   • Invalid state that's a bug                                          │
│   • Failed invariants                                                   │
│   • Impossible code paths                                               │
│   • Initialization failures (Must* pattern)                             │
│                                                                         │
│   NEVER PANIC:                                                          │
│   • In library code for expected failures                               │
│   • For control flow                                                    │
│   • When error is appropriate                                           │
│                                                                         │
│   RULE: When in doubt, use error                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### panic vs error 选择

| 场景 | 使用 |
|------|------|
| 文件/网络错误 | error |
| 用户输入无效 | error |
| 资源未找到 | error |
| nil 指针（不应该是 nil） | panic |
| 程序 bug | panic |
| 违反不变量 | panic |

### Must* 模式

```go
// 返回错误版本
func Parse(s string) (T, error)

// panic 版本用于初始化
func MustParse(s string) T {
    t, err := Parse(s)
    if err != nil {
        panic(err)
    }
    return t
}
```

### 规则总结

1. **调用者可能需要处理** → error
2. **程序有 bug** → panic
3. **不确定** → error（更安全）

### 库代码

- 永远不要为预期失败 panic
- 让调用者决定如何处理
- panic 会杀死调用者的程序

