# Sentinel Errors: Package-Level Error Values

## 1. Engineering Problem

### What real-world problem does this solve?

**Sentinel errors provide well-known error values that callers can check for specific conditions.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SENTINEL ERRORS                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Standard library examples:                                            │
│   ─────────────────────────                                             │
│                                                                         │
│   io.EOF              → End of input stream                             │
│   os.ErrNotExist      → File does not exist                            │
│   os.ErrPermission    → Permission denied                              │
│   sql.ErrNoRows       → Query returned no rows                         │
│   context.Canceled    → Context was canceled                           │
│   context.DeadlineExceeded → Timeout                                   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Usage pattern:                                                        │
│                                                                         │
│   data, err := reader.Read(buf)                                         │
│   if errors.Is(err, io.EOF) {                                          │
│       // Normal end of stream, not an error                            │
│   }                                                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   When to use:                                                          │
│   • Expected conditions callers need to handle                          │
│   • Stable across versions (API contract)                               │
│   • Simple conditions (not parameterized)                               │
│                                                                         │
│   When NOT to use:                                                      │
│   • Error needs context (use custom error type)                         │
│   • Error is internal implementation detail                             │
│   • Too many variants (use error types instead)                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Defining sentinel errors

```go
package route

import "errors"

// Sentinel errors - exported, documented, stable API
var (
    // ErrNotFound is returned when a route cannot be found.
    ErrNotFound = errors.New("route: not found")
    
    // ErrInvalidPrefix is returned when a route prefix is malformed.
    ErrInvalidPrefix = errors.New("route: invalid prefix")
    
    // ErrVrfNotExist is returned when the specified VRF does not exist.
    ErrVrfNotExist = errors.New("route: VRF does not exist")
    
    // ErrDuplicate is returned when adding a route that already exists.
    ErrDuplicate = errors.New("route: already exists")
)
```

### Using sentinel errors

```go
route, err := store.Get(key)
if errors.Is(err, route.ErrNotFound) {
    // Create new route
    return store.Create(newRoute)
}
if err != nil {
    return err  // Propagate other errors
}
// Use route...
```

---

## 3. Language Mechanism

### errors.Is with wrapped errors

```go
// Sentinel error
var ErrNotFound = errors.New("not found")

// Function that wraps
func GetRoute(key string) (Route, error) {
    r, err := store.Get(key)
    if err != nil {
        return Route{}, fmt.Errorf("get route %s: %w", key, err)
    }
    return r, nil
}

// Caller can still check
_, err := GetRoute("10.0.0.0/24")
if errors.Is(err, ErrNotFound) {
    // Works even though error was wrapped!
}
```

### Custom Is method

```go
// When you want flexible matching
type ErrVrfNotFound struct {
    VrfID uint32
}

func (e ErrVrfNotFound) Error() string {
    return fmt.Sprintf("VRF %d not found", e.VrfID)
}

// Any ErrVrfNotFound matches
func (e ErrVrfNotFound) Is(target error) bool {
    _, ok := target.(ErrVrfNotFound)
    return ok
}

// Usage
err := ErrVrfNotFound{VrfID: 42}
errors.Is(err, ErrVrfNotFound{})  // true (any VrfID matches)
```

---

## 4. Idiomatic Usage

### Package organization

```go
// errors.go - define all package errors
package route

import "errors"

// Sentinel errors
var (
    ErrNotFound     = errors.New("route: not found")
    ErrInvalid      = errors.New("route: invalid")
    ErrVrfNotExist  = errors.New("route: VRF not exist")
)

// Error types for errors needing context
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("route: validation: %s: %s", e.Field, e.Message)
}
```

### Documentation

```go
// Get retrieves a route by key.
//
// Returns ErrNotFound if the route does not exist.
// Returns ErrVrfNotExist if the VRF is not configured.
func (s *Store) Get(vrfID uint32, prefix string) (Route, error) {
    if !s.vrfExists(vrfID) {
        return Route{}, ErrVrfNotExist
    }
    
    r, ok := s.routes[key(vrfID, prefix)]
    if !ok {
        return Route{}, ErrNotFound
    }
    return r, nil
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Comparing with ==

```go
// BAD: Breaks when error is wrapped
if err == ErrNotFound {
    // Won't match wrapped errors!
}

// GOOD: Works with wrapped errors
if errors.Is(err, ErrNotFound) {
    // Matches even if wrapped
}
```

### Pitfall 2: Sentinel errors with context

```go
// BAD: Sentinel can't carry context
var ErrNotFound = errors.New("not found")

func GetRoute(key string) (Route, error) {
    if !exists {
        return Route{}, ErrNotFound  // Which key? Unknown!
    }
}

// GOOD: Wrap to add context
func GetRoute(key string) (Route, error) {
    if !exists {
        return Route{}, fmt.Errorf("route %s: %w", key, ErrNotFound)
    }
}
```

### Pitfall 3: Too many sentinels

```go
// BAD: Every condition has a sentinel
var (
    ErrPrefixTooShort = errors.New("prefix too short")
    ErrPrefixTooLong  = errors.New("prefix too long")
    ErrPrefixInvalid  = errors.New("prefix invalid")
    ErrNextHopEmpty   = errors.New("next hop empty")
    // ... 50 more
)

// GOOD: Use error type for parameterized errors
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("%s: %s", e.Field, e.Message)
}
```

---

## 6. Complete Example

```go
package main

import (
    "errors"
    "fmt"
    "sync"
)

// Sentinel errors - stable API
var (
    ErrNotFound    = errors.New("route: not found")
    ErrVrfNotExist = errors.New("route: VRF not exist")
    ErrDuplicate   = errors.New("route: duplicate")
)

// Error type for validation errors
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("route: validation: %s: %s", e.Field, e.Message)
}

// Allow matching any ValidationError
func (e ValidationError) Is(target error) bool {
    _, ok := target.(ValidationError)
    return ok
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type RouteStore struct {
    mu     sync.RWMutex
    vrfs   map[uint32]bool
    routes map[string]Route
}

func NewRouteStore() *RouteStore {
    return &RouteStore{
        vrfs:   map[uint32]bool{1: true, 2: true},
        routes: make(map[string]Route),
    }
}

func (s *RouteStore) key(vrfID uint32, prefix string) string {
    return fmt.Sprintf("%d:%s", vrfID, prefix)
}

// Add adds a route.
// Returns ErrVrfNotExist if VRF doesn't exist.
// Returns ErrDuplicate if route already exists.
// Returns ValidationError if route is invalid.
func (s *RouteStore) Add(r Route) error {
    // Validate
    if r.Prefix == "" {
        return ValidationError{Field: "prefix", Message: "required"}
    }
    if r.NextHop == "" {
        return ValidationError{Field: "next_hop", Message: "required"}
    }
    
    s.mu.Lock()
    defer s.mu.Unlock()
    
    // Check VRF
    if !s.vrfs[r.VrfID] {
        return fmt.Errorf("add route: %w", ErrVrfNotExist)
    }
    
    // Check duplicate
    key := s.key(r.VrfID, r.Prefix)
    if _, ok := s.routes[key]; ok {
        return fmt.Errorf("add route %s: %w", r.Prefix, ErrDuplicate)
    }
    
    s.routes[key] = r
    return nil
}

// Get retrieves a route.
// Returns ErrNotFound if route doesn't exist.
// Returns ErrVrfNotExist if VRF doesn't exist.
func (s *RouteStore) Get(vrfID uint32, prefix string) (Route, error) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    if !s.vrfs[vrfID] {
        return Route{}, fmt.Errorf("get route: %w", ErrVrfNotExist)
    }
    
    key := s.key(vrfID, prefix)
    r, ok := s.routes[key]
    if !ok {
        return Route{}, fmt.Errorf("get route %s: %w", prefix, ErrNotFound)
    }
    return r, nil
}

func main() {
    store := NewRouteStore()
    
    // Add valid route
    err := store.Add(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    if err != nil {
        fmt.Printf("Add error: %v\n", err)
    }
    
    // Try to add duplicate
    err = store.Add(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.2"})
    if errors.Is(err, ErrDuplicate) {
        fmt.Println("Caught: duplicate route")
    }
    
    // Try invalid VRF
    err = store.Add(Route{VrfID: 99, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1"})
    if errors.Is(err, ErrVrfNotExist) {
        fmt.Println("Caught: VRF doesn't exist")
    }
    
    // Try invalid route
    err = store.Add(Route{VrfID: 1, Prefix: "", NextHop: "10.0.0.1"})
    if errors.Is(err, ValidationError{}) {
        fmt.Println("Caught: validation error")
        var valErr ValidationError
        if errors.As(err, &valErr) {
            fmt.Printf("  Field: %s, Message: %s\n", valErr.Field, valErr.Message)
        }
    }
    
    // Get non-existent
    _, err = store.Get(1, "192.168.0.0/24")
    if errors.Is(err, ErrNotFound) {
        fmt.Println("Caught: not found")
    }
    
    // Get existing
    route, err := store.Get(1, "10.0.0.0/24")
    if err == nil {
        fmt.Printf("Found: %+v\n", route)
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SENTINEL ERROR RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE FOR EXPECTED CONDITIONS                                        │
│      • EOF, NotFound, Timeout                                           │
│      • Conditions callers need to handle specifically                   │
│                                                                         │
│   2. USE errors.Is FOR COMPARISON                                       │
│      • Works with wrapped errors                                        │
│      • Never use == for comparison                                      │
│                                                                         │
│   3. WRAP TO ADD CONTEXT                                                │
│      • fmt.Errorf("operation: %w", ErrSentinel)                        │
│      • Sentinel provides type, wrap provides context                    │
│                                                                         │
│   4. DOCUMENT IN FUNCTION COMMENTS                                      │
│      • "Returns ErrNotFound if..."                                      │
│      • Part of API contract                                             │
│                                                                         │
│   5. USE ERROR TYPES FOR PARAMETERIZED ERRORS                           │
│      • When error needs to carry data                                   │
│      • Implement Is() for flexible matching                             │
│                                                                         │
│   6. KEEP NUMBER OF SENTINELS SMALL                                     │
│      • Only for conditions callers must handle                          │
│      • Too many = use error types instead                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 哨兵错误

**哨兵错误是包级别的已知错误值，调用者可以检查特定条件。**

### 标准库示例

| 错误 | 含义 |
|------|------|
| `io.EOF` | 输入流结束 |
| `os.ErrNotExist` | 文件不存在 |
| `context.Canceled` | 上下文已取消 |
| `sql.ErrNoRows` | 查询无结果 |

### 定义和使用

```go
// 定义
var ErrNotFound = errors.New("not found")

// 使用
if errors.Is(err, ErrNotFound) {
    // 处理未找到
}
```

### 何时使用

- ✅ 预期的、需要处理的条件
- ✅ 稳定的 API 契约
- ✅ 简单条件（无参数）

- ❌ 需要携带上下文
- ❌ 内部实现细节
- ❌ 太多变体

### 最佳实践

1. 用 `errors.Is` 比较
2. 包装以添加上下文
3. 在函数注释中文档化
4. 保持数量少

