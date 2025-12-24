# Error Wrapping: errors.Is and errors.As

## 1. Engineering Problem

### What real-world problem does this solve?

**Error wrapping preserves the error chain while adding context at each layer.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR CHAIN                                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without wrapping:                 With wrapping:                      │
│   ─────────────────                 ──────────────                      │
│                                                                         │
│   return err                        return fmt.Errorf(                  │
│   // "connection refused"               "add route to VRF %d: %w",      │
│   // Lost: where? what operation?       vrfID, err)                     │
│                                     // "add route to VRF 1: connect     │
│                                     //  to db: connection refused"      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Error chain visualization:                                            │
│                                                                         │
│   "add route to VRF 1: connect to db: connection refused"              │
│        │                                                                │
│        └─► Unwrap() ─► "connect to db: connection refused"             │
│                              │                                          │
│                              └─► Unwrap() ─► syscall.ECONNREFUSED      │
│                                                                         │
│   errors.Is(err, syscall.ECONNREFUSED) → true at any level             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### %w verb creates chain

```go
// Creates wrapped error chain
if err := db.Connect(); err != nil {
    return fmt.Errorf("connect to db: %w", err)  // Chain preserved
}

// %v breaks the chain
if err := db.Connect(); err != nil {
    return fmt.Errorf("connect to db: %v", err)  // Chain BROKEN
}
```

### errors.Is vs errors.As

```go
// errors.Is: Check if chain CONTAINS specific error VALUE
if errors.Is(err, os.ErrNotExist) {
    // Somewhere in the chain is os.ErrNotExist
}

// errors.As: EXTRACT specific error TYPE from chain
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    // Found PathError, now have access to pathErr.Path, etc.
}
```

---

## 3. Language Mechanism

### Implementing Unwrap

```go
type RouteError struct {
    Op     string
    VrfID  uint32
    Prefix string
    Err    error  // The wrapped error
}

func (e *RouteError) Error() string {
    if e.Err != nil {
        return fmt.Sprintf("%s: vrf=%d prefix=%s: %v",
            e.Op, e.VrfID, e.Prefix, e.Err)
    }
    return fmt.Sprintf("%s: vrf=%d prefix=%s", e.Op, e.VrfID, e.Prefix)
}

// Implement Unwrap to enable errors.Is/As
func (e *RouteError) Unwrap() error {
    return e.Err
}
```

### Multi-error unwrapping (Go 1.20+)

```go
type MultiError struct {
    Errors []error
}

func (m *MultiError) Error() string {
    // Format all errors
}

// Return multiple wrapped errors
func (m *MultiError) Unwrap() []error {
    return m.Errors
}

// errors.Is checks ALL wrapped errors
errors.Is(multiErr, ErrNotFound)  // True if ANY contains ErrNotFound
```

---

## 4. Idiomatic Usage

### Wrap at boundaries

```go
// Repository layer
func (r *RouteRepo) Get(key string) (Route, error) {
    row := r.db.QueryRow("SELECT * FROM routes WHERE key = ?", key)
    var route Route
    if err := row.Scan(&route.VrfID, &route.Prefix); err != nil {
        return Route{}, fmt.Errorf("query route %s: %w", key, err)
    }
    return route, nil
}

// Service layer
func (s *RouteService) GetRoute(vrfID uint32, prefix string) (Route, error) {
    route, err := s.repo.Get(key(vrfID, prefix))
    if err != nil {
        return Route{}, fmt.Errorf("route service: %w", err)
    }
    return route, nil
}

// Error message: "route service: query route 1:10.0.0.0/24: sql: no rows"
// But errors.Is(err, sql.ErrNoRows) still works!
```

### Custom Is for flexible matching

```go
type VrfError struct {
    VrfID uint32
    Err   error
}

func (e VrfError) Error() string {
    return fmt.Sprintf("VRF %d: %v", e.VrfID, e.Err)
}

func (e VrfError) Unwrap() error {
    return e.Err
}

// Match any VrfError (regardless of VrfID)
func (e VrfError) Is(target error) bool {
    _, ok := target.(VrfError)
    return ok
}

// Usage
err := VrfError{VrfID: 42, Err: ErrNotFound}
errors.Is(err, VrfError{})  // true - matches any VrfError
errors.Is(err, ErrNotFound) // true - via Unwrap
```

---

## 5. Common Pitfalls

### Pitfall 1: Using %v instead of %w

```go
// BAD: Chain broken, errors.Is won't work
return fmt.Errorf("failed: %v", err)

// GOOD: Chain preserved
return fmt.Errorf("failed: %w", err)
```

### Pitfall 2: Wrong target type for errors.As

```go
// BAD: Target must be pointer to error type pointer
var pathErr os.PathError  // Wrong!
errors.As(err, &pathErr)  // Won't work

// GOOD: Pointer to pointer
var pathErr *os.PathError
errors.As(err, &pathErr)  // Correct
```

### Pitfall 3: Checking wrapped nil

```go
// BAD: Creates non-nil interface with nil concrete value
func getError() error {
    var err *RouteError = nil
    return fmt.Errorf("context: %w", err)  // Returns non-nil!
}

// GOOD: Return nil explicitly
func getError() error {
    var err *RouteError = nil
    if err == nil {
        return nil
    }
    return fmt.Errorf("context: %w", err)
}
```

---

## 6. Complete Example

```go
package main

import (
    "errors"
    "fmt"
)

// Sentinel errors
var (
    ErrNotFound = errors.New("not found")
    ErrInvalid  = errors.New("invalid")
)

// Custom error type
type RouteError struct {
    Op     string
    VrfID  uint32
    Prefix string
    Err    error
}

func (e *RouteError) Error() string {
    if e.Err != nil {
        return fmt.Sprintf("%s: vrf=%d prefix=%s: %v",
            e.Op, e.VrfID, e.Prefix, e.Err)
    }
    return fmt.Sprintf("%s: vrf=%d prefix=%s", e.Op, e.VrfID, e.Prefix)
}

func (e *RouteError) Unwrap() error {
    return e.Err
}

// Storage layer
func dbGet(key string) error {
    return ErrNotFound  // Simulated DB error
}

// Repository layer - wraps DB errors
func repoGetRoute(vrfID uint32, prefix string) error {
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    if err := dbGet(key); err != nil {
        return &RouteError{
            Op:     "get",
            VrfID:  vrfID,
            Prefix: prefix,
            Err:    err,
        }
    }
    return nil
}

// Service layer - wraps repo errors
func serviceGetRoute(vrfID uint32, prefix string) error {
    if err := repoGetRoute(vrfID, prefix); err != nil {
        return fmt.Errorf("route service: %w", err)
    }
    return nil
}

func main() {
    err := serviceGetRoute(1, "10.0.0.0/24")
    
    fmt.Printf("Full error: %v\n", err)
    // Output: route service: get: vrf=1 prefix=10.0.0.0/24: not found
    
    // Check sentinel with errors.Is
    if errors.Is(err, ErrNotFound) {
        fmt.Println("✓ errors.Is(err, ErrNotFound) = true")
    }
    
    // Extract custom error with errors.As
    var routeErr *RouteError
    if errors.As(err, &routeErr) {
        fmt.Printf("✓ Found RouteError: op=%s vrf=%d prefix=%s\n",
            routeErr.Op, routeErr.VrfID, routeErr.Prefix)
    }
    
    // Unwrap manually
    fmt.Println("\nError chain:")
    for e := err; e != nil; e = errors.Unwrap(e) {
        fmt.Printf("  → %T: %v\n", e, e)
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR WRAPPING RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE %w TO PRESERVE CHAIN                                           │
│      • fmt.Errorf("context: %w", err)                                   │
│      • Never %v if caller needs to check                                │
│                                                                         │
│   2. IMPLEMENT Unwrap() FOR CUSTOM ERRORS                               │
│      • Returns the wrapped error                                        │
│      • Enables errors.Is/As to traverse                                 │
│                                                                         │
│   3. errors.Is FOR VALUE COMPARISON                                     │
│      • Sentinel errors                                                  │
│      • Works through entire chain                                       │
│                                                                         │
│   4. errors.As FOR TYPE EXTRACTION                                      │
│      • Custom error types                                               │
│      • Target must be **ErrorType                                       │
│                                                                         │
│   5. WRAP AT LAYER BOUNDARIES                                           │
│      • Repository, service, handler                                     │
│      • Each adds its context                                            │
│                                                                         │
│   6. IMPLEMENT Is() FOR FLEXIBLE MATCHING                               │
│      • When any instance should match                                   │
│      • Ignores field values                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 错误包装

**错误包装在添加上下文的同时保留错误链。**

### %w vs %v

| 格式 | 效果 |
|------|------|
| `%w` | 包装错误，保留链 |
| `%v` | 格式化为字符串，断开链 |

### errors.Is vs errors.As

| 函数 | 用途 | 示例 |
|------|------|------|
| `errors.Is` | 值比较 | `errors.Is(err, ErrNotFound)` |
| `errors.As` | 类型提取 | `errors.As(err, &routeErr)` |

### 实现 Unwrap

```go
type MyError struct {
    Err error
}

func (e *MyError) Unwrap() error {
    return e.Err
}
```

### 最佳实践

1. 用 `%w` 保留错误链
2. 实现 `Unwrap()` 允许遍历
3. 在层边界添加上下文
4. 用 `errors.Is` 检查哨兵错误
5. 用 `errors.As` 提取自定义类型

