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
│   Error chain:                                                          │
│                                                                         │
│   ┌─────────────────────────────────────────────────────┐               │
│   │  "add route to VRF 1: connect to db: conn refused"  │               │
│   └──────────────────────────┬──────────────────────────┘               │
│                              │ Unwrap()                                 │
│                              ▼                                          │
│   ┌─────────────────────────────────────────────────────┐               │
│   │  "connect to db: connection refused"                │               │
│   └──────────────────────────┬──────────────────────────┘               │
│                              │ Unwrap()                                 │
│                              ▼                                          │
│   ┌─────────────────────────────────────────────────────┐               │
│   │  "connection refused" (syscall.ECONNREFUSED)        │               │
│   └─────────────────────────────────────────────────────┘               │
│                                                                         │
│   errors.Is(err, syscall.ECONNREFUSED) → true at any level             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### %w verb wraps errors

```go
// Wrap with context
if err := db.Connect(); err != nil {
    return fmt.Errorf("connect to db: %w", err)
}

// %w wraps (chain preserved)
// %v formats (chain broken)
```

### errors.Is vs errors.As

```go
// errors.Is: Check if error chain contains specific error VALUE
if errors.Is(err, os.ErrNotExist) {
    // File doesn't exist
}

// errors.As: Extract specific error TYPE from chain
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    fmt.Println("Path:", pathErr.Path)
}
```

---

## 3. Language Mechanism

### Wrapping with fmt.Errorf

```go
import (
    "errors"
    "fmt"
)

func addRoute(vrfID uint32, prefix string) error {
    if err := validatePrefix(prefix); err != nil {
        return fmt.Errorf("add route %s: %w", prefix, err)
    }
    if err := db.Insert(prefix); err != nil {
        return fmt.Errorf("add route %s: %w", prefix, err)
    }
    return nil
}
```

### Custom error with Unwrap

```go
type RouteError struct {
    Op     string
    VrfID  uint32
    Prefix string
    Err    error  // Wrapped error
}

func (e *RouteError) Error() string {
    return fmt.Sprintf("%s: vrf=%d prefix=%s: %v", e.Op, e.VrfID, e.Prefix, e.Err)
}

// Implement Unwrap to enable errors.Is/As
func (e *RouteError) Unwrap() error {
    return e.Err
}
```

### errors.Is implementation

```go
// errors.Is checks the chain
func Is(err, target error) bool {
    for {
        if err == target {
            return true
        }
        // Check if err has Is method
        if x, ok := err.(interface{ Is(error) bool }); ok {
            if x.Is(target) {
                return true
            }
        }
        // Unwrap and continue
        if err = Unwrap(err); err == nil {
            return false
        }
    }
}
```

### errors.As implementation

```go
// errors.As finds and extracts a specific type
func As(err error, target interface{}) bool {
    // Traverse chain
    for err != nil {
        if reflect.TypeOf(err).AssignableTo(targetType) {
            // Found matching type
            targetPtr.Set(reflect.ValueOf(err))
            return true
        }
        err = Unwrap(err)
    }
    return false
}
```

---

## 4. Idiomatic Usage

### Wrap at meaningful boundaries

```go
// Service layer adds service context
func (s *RouteService) AddRoute(r Route) error {
    if err := s.validator.Validate(r); err != nil {
        return fmt.Errorf("route service: %w", err)
    }
    if err := s.store.Save(r); err != nil {
        return fmt.Errorf("route service: save: %w", err)
    }
    return nil
}

// Repository layer adds storage context
func (s *RouteStore) Save(r Route) error {
    if err := s.db.Insert(r); err != nil {
        return fmt.Errorf("save route %s: %w", r.Prefix, err)
    }
    return nil
}
```

### Custom Is method for equivalence

```go
type ErrVrfNotFound struct {
    VrfID uint32
}

func (e ErrVrfNotFound) Error() string {
    return fmt.Sprintf("VRF %d not found", e.VrfID)
}

// Match any ErrVrfNotFound regardless of VrfID
func (e ErrVrfNotFound) Is(target error) bool {
    _, ok := target.(ErrVrfNotFound)
    return ok
}

// Usage
err := ErrVrfNotFound{VrfID: 42}
fmt.Println(errors.Is(err, ErrVrfNotFound{}))  // true
```

### Sentinel errors vs wrapped errors

```go
// Sentinel error - for specific conditions
var ErrNotFound = errors.New("not found")

// Check with errors.Is
if errors.Is(err, ErrNotFound) {
    // Handle not found
}

// Wrapped error - for context
wrapped := fmt.Errorf("get route: %w", ErrNotFound)
errors.Is(wrapped, ErrNotFound)  // true
```

---

## 5. Common Pitfalls

### Pitfall 1: Using %v instead of %w

```go
// BAD: Chain broken
return fmt.Errorf("failed: %v", err)  // err is just string now
errors.Is(result, originalErr)  // false!

// GOOD: Chain preserved
return fmt.Errorf("failed: %w", err)
errors.Is(result, originalErr)  // true
```

### Pitfall 2: Comparing with ==

```go
// BAD: Breaks when wrapped
if err == ErrNotFound {  // false if wrapped!
    // Handle
}

// GOOD: Works with wrapped errors
if errors.Is(err, ErrNotFound) {
    // Handle
}
```

### Pitfall 3: Wrong target type for As

```go
// BAD: Target must be pointer to pointer
var pathErr os.PathError  // Wrong!
errors.As(err, &pathErr)  // Won't work

// GOOD: Pointer to interface pointer or pointer to struct pointer
var pathErr *os.PathError
errors.As(err, &pathErr)  // Correct
```

### Pitfall 4: Wrapping with nil

```go
// BAD: Creates confusing error
if err == nil {
    return fmt.Errorf("no error: %w", err)  // "no error: <nil>"
}

// GOOD: Return nil explicitly
if err == nil {
    return nil
}
return fmt.Errorf("failed: %w", err)
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
    ErrNotFound     = errors.New("not found")
    ErrInvalidRoute = errors.New("invalid route")
    ErrVrfNotExist  = errors.New("VRF does not exist")
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
        return fmt.Sprintf("%s: vrf=%d prefix=%s: %v", e.Op, e.VrfID, e.Prefix, e.Err)
    }
    return fmt.Sprintf("%s: vrf=%d prefix=%s", e.Op, e.VrfID, e.Prefix)
}

func (e *RouteError) Unwrap() error {
    return e.Err
}

// ValidationError with Is method
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("validation: %s: %s", e.Field, e.Message)
}

// Any ValidationError matches
func (e ValidationError) Is(target error) bool {
    _, ok := target.(ValidationError)
    return ok
}

// Simulated data layer
type RouteStore struct {
    routes map[string]Route
    vrfs   map[uint32]bool
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteStore() *RouteStore {
    return &RouteStore{
        routes: make(map[string]Route),
        vrfs:   map[uint32]bool{1: true, 2: true},
    }
}

func (s *RouteStore) Get(vrfID uint32, prefix string) (Route, error) {
    if !s.vrfs[vrfID] {
        return Route{}, &RouteError{
            Op:    "get",
            VrfID: vrfID,
            Err:   ErrVrfNotExist,
        }
    }
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    route, ok := s.routes[key]
    if !ok {
        return Route{}, &RouteError{
            Op:     "get",
            VrfID:  vrfID,
            Prefix: prefix,
            Err:    ErrNotFound,
        }
    }
    return route, nil
}

func (s *RouteStore) Add(r Route) error {
    if r.Prefix == "" {
        return &RouteError{
            Op:     "add",
            VrfID:  r.VrfID,
            Prefix: r.Prefix,
            Err:    ValidationError{Field: "prefix", Message: "required"},
        }
    }
    
    if !s.vrfs[r.VrfID] {
        return &RouteError{
            Op:    "add",
            VrfID: r.VrfID,
            Err:   ErrVrfNotExist,
        }
    }
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    s.routes[key] = r
    return nil
}

// Service layer wraps store errors
type RouteService struct {
    store *RouteStore
}

func (s *RouteService) GetRoute(vrfID uint32, prefix string) (Route, error) {
    route, err := s.store.Get(vrfID, prefix)
    if err != nil {
        return Route{}, fmt.Errorf("route service: %w", err)
    }
    return route, nil
}

func main() {
    store := NewRouteStore()
    service := &RouteService{store: store}
    
    // Test 1: Valid route
    store.Add(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    
    // Test 2: Not found error
    _, err := service.GetRoute(1, "unknown")
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        
        // Check with errors.Is
        if errors.Is(err, ErrNotFound) {
            fmt.Println("  -> Route not found (errors.Is)")
        }
        
        // Extract with errors.As
        var routeErr *RouteError
        if errors.As(err, &routeErr) {
            fmt.Printf("  -> RouteError: op=%s vrf=%d prefix=%s\n",
                routeErr.Op, routeErr.VrfID, routeErr.Prefix)
        }
    }
    
    // Test 3: VRF not exist
    _, err = service.GetRoute(99, "10.0.0.0/24")
    if err != nil {
        fmt.Printf("\nError: %v\n", err)
        if errors.Is(err, ErrVrfNotExist) {
            fmt.Println("  -> VRF doesn't exist (errors.Is)")
        }
    }
    
    // Test 4: Validation error
    err = store.Add(Route{VrfID: 1, Prefix: ""})
    if err != nil {
        fmt.Printf("\nError: %v\n", err)
        
        // Check if any ValidationError
        if errors.Is(err, ValidationError{}) {
            fmt.Println("  -> Validation error (errors.Is with Is method)")
        }
        
        // Extract specific ValidationError
        var valErr ValidationError
        if errors.As(err, &valErr) {
            fmt.Printf("  -> ValidationError: field=%s message=%s\n",
                valErr.Field, valErr.Message)
        }
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
│      • Don't use %v if you need Is/As                                   │
│                                                                         │
│   2. IMPLEMENT Unwrap() FOR CUSTOM ERRORS                               │
│      • Return the wrapped error                                         │
│      • Enables errors.Is/As to traverse                                 │
│                                                                         │
│   3. USE errors.Is FOR VALUE COMPARISON                                 │
│      • Sentinel errors (ErrNotFound)                                    │
│      • Works through wrapped layers                                     │
│                                                                         │
│   4. USE errors.As FOR TYPE EXTRACTION                                  │
│      • Custom error types (*RouteError)                                 │
│      • Target must be pointer to pointer                                │
│                                                                         │
│   5. IMPLEMENT Is() FOR CUSTOM MATCHING                                 │
│      • When you want type-level matching                                │
│      • Ignores specific field values                                    │
│                                                                         │
│   6. WRAP AT LAYER BOUNDARIES                                           │
│      • Add context without over-wrapping                                │
│      • Each layer adds its perspective                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 错误包装核心概念

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

### 自定义错误实现 Unwrap

```go
type RouteError struct {
    Op  string
    Err error  // 被包装的错误
}

func (e *RouteError) Unwrap() error {
    return e.Err
}
```

### 自定义 Is 方法

```go
type ValidationError struct {
    Field string
}

func (e ValidationError) Is(target error) bool {
    _, ok := target.(ValidationError)
    return ok  // 任何 ValidationError 都匹配
}
```

### 最佳实践

1. **使用 %w 保留错误链**
2. **实现 Unwrap() 允许遍历**
3. **用 errors.Is 检查哨兵错误**
4. **用 errors.As 提取自定义类型**
5. **在层边界添加上下文**

