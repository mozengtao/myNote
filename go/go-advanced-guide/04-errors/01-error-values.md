# Error Values: Go's Error Philosophy

## 1. Engineering Problem

### What real-world problem does this solve?

**Go treats errors as values, not exceptions - making error handling explicit and impossible to ignore.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR HANDLING MODELS                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Exceptions (Java/Python/C++):                                         │
│   ─────────────────────────────                                         │
│                                                                         │
│   try {                            Problems:                            │
│       result = doWork();           • Hidden control flow                │
│   } catch (Exception e) {          • Easy to forget handling            │
│       handleError(e);              • Performance overhead               │
│   }                                • Unclear which functions throw      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Error Values (Go):                                                    │
│   ─────────────────                                                     │
│                                                                         │
│   result, err := doWork()          Benefits:                            │
│   if err != nil {                  • Explicit control flow              │
│       return fmt.Errorf(           • Cannot ignore (compiler warning)   │
│           "work failed: %w", err)  • No runtime overhead                │
│   }                                • Clear which functions can fail     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   The error interface:                                                  │
│                                                                         │
│   type error interface {                                                │
│       Error() string                                                    │
│   }                                                                     │
│                                                                         │
│   Any type with Error() string is an error                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Errors are just values

```go
// The error interface
type error interface {
    Error() string
}

// Errors can be created many ways
err := errors.New("something went wrong")
err := fmt.Errorf("failed to process %s: %w", name, cause)

// Errors can carry additional context
type RouteError struct {
    VrfID  uint32
    Prefix string
    Cause  error
}

func (e *RouteError) Error() string {
    return fmt.Sprintf("route %d:%s: %v", e.VrfID, e.Prefix, e.Cause)
}

func (e *RouteError) Unwrap() error {
    return e.Cause
}
```

### Error handling patterns

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR HANDLING PATTERNS                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. CHECK AND RETURN                                                   │
│   ───────────────────                                                   │
│   if err != nil {                                                       │
│       return err  // Propagate unchanged                                │
│   }                                                                     │
│                                                                         │
│   2. CHECK, WRAP, AND RETURN                                            │
│   ──────────────────────────                                            │
│   if err != nil {                                                       │
│       return fmt.Errorf("context: %w", err)  // Add context             │
│   }                                                                     │
│                                                                         │
│   3. CHECK AND HANDLE                                                   │
│   ───────────────────                                                   │
│   if err != nil {                                                       │
│       log.Printf("error: %v", err)                                      │
│       return defaultValue  // Use fallback                              │
│   }                                                                     │
│                                                                         │
│   4. CHECK SPECIFIC ERROR                                               │
│   ───────────────────────                                               │
│   if errors.Is(err, ErrNotFound) {                                      │
│       return createNew()  // Handle specific case                       │
│   }                                                                     │
│   return err  // Propagate others                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Creating errors

```go
import (
    "errors"
    "fmt"
)

// Simple error
err := errors.New("not found")

// Formatted error
err := fmt.Errorf("route %s not found", prefix)

// Wrapped error (Go 1.13+)
err := fmt.Errorf("failed to add route: %w", originalErr)
```

### Error wrapping and unwrapping

```go
// Wrap with context
func AddRoute(r Route) error {
    if err := validate(r); err != nil {
        return fmt.Errorf("add route %s: %w", r.Prefix, err)
    }
    // ...
}

// Check wrapped errors
if errors.Is(err, ErrInvalidPrefix) {
    // Handle invalid prefix
}

// Get wrapped error of specific type
var routeErr *RouteError
if errors.As(err, &routeErr) {
    fmt.Println("VRF:", routeErr.VrfID)
}

// Unwrap manually
unwrapped := errors.Unwrap(err)
```

### Sentinel errors

```go
// Package-level error values
var (
    ErrNotFound     = errors.New("not found")
    ErrInvalidRoute = errors.New("invalid route")
    ErrVrfNotExist  = errors.New("VRF does not exist")
)

// Usage
func GetRoute(key string) (Route, error) {
    route, ok := routes[key]
    if !ok {
        return Route{}, ErrNotFound
    }
    return route, nil
}

// Checking
if errors.Is(err, ErrNotFound) {
    // Create new route
}
```

---

## 4. Idiomatic Usage

### Wrap at boundaries

```go
// Add context at package/layer boundaries
func (s *RouteService) AddRoute(r Route) error {
    if err := s.repo.Save(r); err != nil {
        // Wrap with service-level context
        return fmt.Errorf("route service: %w", err)
    }
    return nil
}

func (r *RouteRepo) Save(route Route) error {
    if err := r.db.Insert(route); err != nil {
        // Wrap with repo-level context
        return fmt.Errorf("save route %s: %w", route.Prefix, err)
    }
    return nil
}
```

### Custom error types

```go
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("validation: %s: %s", e.Field, e.Message)
}

func validateRoute(r Route) error {
    if r.Prefix == "" {
        return ValidationError{Field: "prefix", Message: "required"}
    }
    return nil
}

// Type assertion
var valErr ValidationError
if errors.As(err, &valErr) {
    fmt.Printf("Field %s is invalid\n", valErr.Field)
}
```

### errors.Is vs errors.As

```go
// errors.Is: Compare with sentinel errors (value comparison)
if errors.Is(err, ErrNotFound) {
    // Handle not found
}

// errors.As: Get error of specific type (type assertion)
var routeErr *RouteError
if errors.As(err, &routeErr) {
    // Use routeErr.VrfID, routeErr.Prefix, etc.
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Comparing errors with ==

```go
// BAD: Breaks when error is wrapped
if err == ErrNotFound {  // Might be false even if wrapped
    // Handle
}

// GOOD: Works with wrapped errors
if errors.Is(err, ErrNotFound) {
    // Handle
}
```

### Pitfall 2: Ignoring errors

```go
// BAD: Silently ignoring error
data, _ := json.Marshal(route)

// BAD: Ignoring in defer
defer file.Close()  // Close can fail!

// GOOD: Handle or explicitly document
data, err := json.Marshal(route)
if err != nil {
    return fmt.Errorf("marshal route: %w", err)
}

// GOOD: Named return for defer error handling
func writeRoute(r Route) (err error) {
    f, err := os.Create("route.json")
    if err != nil {
        return err
    }
    defer func() {
        if cerr := f.Close(); cerr != nil && err == nil {
            err = cerr
        }
    }()
    // ...
}
```

### Pitfall 3: Over-wrapping

```go
// BAD: Too much wrapping, unreadable
return fmt.Errorf("handler: %w",
    fmt.Errorf("service: %w",
        fmt.Errorf("repo: %w", err)))

// Error: "handler: service: repo: connection refused"

// GOOD: Wrap at meaningful boundaries only
// handler doesn't wrap, service adds context
return fmt.Errorf("add route to VRF %d: %w", vrfID, err)
```

### Pitfall 4: Returning nil error with non-nil value

```go
// BAD: Returns (*MyError, nil) not nil!
func doWork() error {
    var err *MyError = nil
    return err  // Interface is (type=*MyError, value=nil), != nil
}

// GOOD: Return explicit nil
func doWork() error {
    var err *MyError = nil
    if err == nil {
        return nil
    }
    return err
}
```

---

## 6. Complete Example

```go
package main

import (
    "errors"
    "fmt"
    "strings"
)

// Sentinel errors
var (
    ErrNotFound     = errors.New("not found")
    ErrInvalidRoute = errors.New("invalid route")
    ErrVrfNotExist  = errors.New("VRF does not exist")
)

// Custom error type with context
type RouteError struct {
    Op      string  // Operation that failed
    VrfID   uint32
    Prefix  string
    Cause   error
}

func (e *RouteError) Error() string {
    if e.Cause != nil {
        return fmt.Sprintf("%s: vrf=%d prefix=%s: %v", e.Op, e.VrfID, e.Prefix, e.Cause)
    }
    return fmt.Sprintf("%s: vrf=%d prefix=%s", e.Op, e.VrfID, e.Prefix)
}

func (e *RouteError) Unwrap() error {
    return e.Cause
}

// ValidationError for input validation
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("validation error: %s: %s", e.Field, e.Message)
}

// Route model
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// RouteStore simulates storage
type RouteStore struct {
    vrfs   map[uint32]bool
    routes map[string]Route
}

func NewRouteStore() *RouteStore {
    return &RouteStore{
        vrfs:   map[uint32]bool{1: true, 2: true},
        routes: make(map[string]Route),
    }
}

func (s *RouteStore) AddRoute(r Route) error {
    // Validate
    if err := validateRoute(r); err != nil {
        return &RouteError{
            Op:     "add",
            VrfID:  r.VrfID,
            Prefix: r.Prefix,
            Cause:  err,
        }
    }
    
    // Check VRF exists
    if !s.vrfs[r.VrfID] {
        return &RouteError{
            Op:     "add",
            VrfID:  r.VrfID,
            Prefix: r.Prefix,
            Cause:  ErrVrfNotExist,
        }
    }
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    s.routes[key] = r
    return nil
}

func (s *RouteStore) GetRoute(vrfID uint32, prefix string) (Route, error) {
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := s.routes[key]
    if !ok {
        return Route{}, &RouteError{
            Op:     "get",
            VrfID:  vrfID,
            Prefix: prefix,
            Cause:  ErrNotFound,
        }
    }
    return r, nil
}

func validateRoute(r Route) error {
    if r.Prefix == "" {
        return ValidationError{Field: "prefix", Message: "required"}
    }
    if !strings.Contains(r.Prefix, "/") {
        return ValidationError{Field: "prefix", Message: "must be CIDR format"}
    }
    if r.NextHop == "" {
        return ValidationError{Field: "next_hop", Message: "required"}
    }
    return nil
}

func main() {
    store := NewRouteStore()
    
    // Valid route
    err := store.AddRoute(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    if err != nil {
        fmt.Printf("Error: %v\n", err)
    }
    
    // Invalid route - validation error
    err = store.AddRoute(Route{VrfID: 1, Prefix: "", NextHop: "192.168.1.1"})
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        
        // Check if validation error
        var valErr ValidationError
        if errors.As(err, &valErr) {
            fmt.Printf("  -> Validation failed: field=%s\n", valErr.Field)
        }
    }
    
    // Invalid VRF
    err = store.AddRoute(Route{VrfID: 99, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        
        // Check if VRF doesn't exist
        if errors.Is(err, ErrVrfNotExist) {
            fmt.Printf("  -> VRF doesn't exist\n")
        }
    }
    
    // Get existing route
    route, err := store.GetRoute(1, "10.0.0.0/24")
    if err != nil {
        fmt.Printf("Error: %v\n", err)
    } else {
        fmt.Printf("Found: %+v\n", route)
    }
    
    // Get non-existent route
    _, err = store.GetRoute(1, "172.16.0.0/16")
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        
        if errors.Is(err, ErrNotFound) {
            fmt.Printf("  -> Route not found\n")
        }
        
        // Get the RouteError for more context
        var routeErr *RouteError
        if errors.As(err, &routeErr) {
            fmt.Printf("  -> Operation: %s, VRF: %d\n", routeErr.Op, routeErr.VrfID)
        }
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR HANDLING RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ALWAYS CHECK ERRORS                                                │
│      • Never ignore returned errors                                     │
│      • Use blank identifier only with explicit comment                  │
│                                                                         │
│   2. WRAP AT BOUNDARIES                                                 │
│      • Add context when crossing package/layer boundaries               │
│      • Use %w to preserve error chain                                   │
│      • Don't over-wrap                                                  │
│                                                                         │
│   3. USE errors.Is AND errors.As                                        │
│      • errors.Is for sentinel error comparison                          │
│      • errors.As for type assertion                                     │
│      • Never use == for error comparison                                │
│                                                                         │
│   4. DEFINE ERRORS AT PACKAGE LEVEL                                     │
│      • Sentinel errors are exported                                     │
│      • Custom error types when context needed                           │
│      • Implement Unwrap() for wrapping                                  │
│                                                                         │
│   5. ERROR MESSAGES                                                     │
│      • Lowercase, no punctuation                                        │
│      • Add context, not duplication                                     │
│      • Be specific about what failed                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 错误是值

**Go 把错误当作值处理，而非异常。错误处理是显式的。**

### error 接口

```go
type error interface {
    Error() string
}
```

任何实现 `Error() string` 方法的类型都是 error。

### 关键函数

| 函数 | 用途 |
|------|------|
| `errors.New(msg)` | 创建简单错误 |
| `fmt.Errorf("...: %w", err)` | 包装错误（保留链） |
| `errors.Is(err, target)` | 检查错误链中是否包含 target |
| `errors.As(err, &target)` | 从错误链中提取特定类型 |
| `errors.Unwrap(err)` | 获取被包装的错误 |

### 最佳实践

1. **总是检查错误**：不要忽略返回的错误
2. **在边界处包装**：跨包/层时添加上下文
3. **使用 errors.Is/As**：不要用 == 比较
4. **包级别定义错误**：哨兵错误应导出
5. **错误消息**：小写，无标点，具体说明失败原因

### 常见陷阱

1. **用 == 比较错误**：包装后会失败
2. **忽略错误**：可能导致静默失败
3. **过度包装**：错误消息变得不可读
4. **nil 接口陷阱**：返回类型化的 nil 不等于 nil

