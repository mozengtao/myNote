# Testing Fundamentals: go test and Patterns

## 1. Engineering Problem

### What real-world problem does this solve?

**Go's built-in testing framework is simple, powerful, and requires no external dependencies.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GO TESTING MODEL                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   File convention:                                                      │
│   ─────────────────                                                     │
│   route.go       → route_test.go                                        │
│   handler.go     → handler_test.go                                      │
│                                                                         │
│   Function naming:                                                      │
│   ─────────────────                                                     │
│   func TestXxx(t *testing.T)       // Test                              │
│   func BenchmarkXxx(b *testing.B)  // Benchmark                         │
│   func ExampleXxx()                // Runnable example in docs          │
│   func FuzzXxx(f *testing.F)       // Fuzz test (Go 1.18+)             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Running tests:                                                        │
│                                                                         │
│   go test              # Run tests in current package                   │
│   go test ./...        # Run tests in all packages                      │
│   go test -v           # Verbose output                                 │
│   go test -race        # Enable race detector                           │
│   go test -cover       # Show coverage                                  │
│   go test -run TestXxx # Run specific test                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Table-driven tests

```go
func TestParseRoute(t *testing.T) {
    tests := []struct {
        name    string
        input   string
        want    Route
        wantErr bool
    }{
        {
            name:  "valid IPv4",
            input: "10.0.0.0/24",
            want:  Route{Prefix: "10.0.0.0/24", IsV6: false},
        },
        {
            name:  "valid IPv6",
            input: "2001:db8::/32",
            want:  Route{Prefix: "2001:db8::/32", IsV6: true},
        },
        {
            name:    "invalid format",
            input:   "not-a-route",
            wantErr: true,
        },
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := ParseRoute(tt.input)
            if (err != nil) != tt.wantErr {
                t.Errorf("error = %v, wantErr %v", err, tt.wantErr)
                return
            }
            if got != tt.want {
                t.Errorf("got %v, want %v", got, tt.want)
            }
        })
    }
}
```

---

## 3. Language Mechanism

### Basic test

```go
package route

import "testing"

func TestAdd(t *testing.T) {
    rm := NewRouteManager()
    
    err := rm.Add(Route{Prefix: "10.0.0.0/24"})
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
    
    if rm.Count() != 1 {
        t.Errorf("count = %d, want 1", rm.Count())
    }
}
```

### t.Run for subtests

```go
func TestRouteManager(t *testing.T) {
    rm := NewRouteManager()
    
    t.Run("Add", func(t *testing.T) {
        err := rm.Add(Route{Prefix: "10.0.0.0/24"})
        if err != nil {
            t.Fatalf("Add failed: %v", err)
        }
    })
    
    t.Run("Get", func(t *testing.T) {
        route, ok := rm.Get("10.0.0.0/24")
        if !ok {
            t.Fatal("Get returned false")
        }
        if route.Prefix != "10.0.0.0/24" {
            t.Errorf("prefix = %q, want %q", route.Prefix, "10.0.0.0/24")
        }
    })
    
    t.Run("Delete", func(t *testing.T) {
        rm.Delete("10.0.0.0/24")
        if rm.Count() != 0 {
            t.Errorf("count = %d, want 0", rm.Count())
        }
    })
}
```

### t.Parallel for concurrent tests

```go
func TestParallel(t *testing.T) {
    tests := []struct {
        name string
        // ...
    }{
        {name: "case1"},
        {name: "case2"},
        {name: "case3"},
    }
    
    for _, tt := range tests {
        tt := tt  // Capture range variable!
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel()  // Run in parallel
            // test logic
        })
    }
}
```

### Test helpers

```go
func TestWithHelper(t *testing.T) {
    rm := setupRouteManager(t)  // Helper
    
    // Test logic
}

func setupRouteManager(t *testing.T) *RouteManager {
    t.Helper()  // Mark as helper for better error reporting
    
    rm := NewRouteManager()
    if err := rm.Load("testdata/routes.json"); err != nil {
        t.Fatalf("setup failed: %v", err)
    }
    return rm
}
```

### Cleanup

```go
func TestWithCleanup(t *testing.T) {
    f, err := os.CreateTemp("", "test-*")
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() {
        os.Remove(f.Name())
    })
    
    // Use f...
}
```

---

## 4. Idiomatic Usage

### Testing errors

```go
func TestErrors(t *testing.T) {
    rm := NewRouteManager()
    
    // Test specific error
    _, err := rm.Get("nonexistent")
    if !errors.Is(err, ErrNotFound) {
        t.Errorf("error = %v, want ErrNotFound", err)
    }
    
    // Test error type
    var routeErr *RouteError
    if !errors.As(err, &routeErr) {
        t.Error("expected RouteError")
    }
}
```

### Testing with interfaces

```go
// Mock implementation
type mockStore struct {
    routes map[string]Route
    err    error
}

func (m *mockStore) Get(key string) (Route, error) {
    if m.err != nil {
        return Route{}, m.err
    }
    r, ok := m.routes[key]
    if !ok {
        return Route{}, ErrNotFound
    }
    return r, nil
}

func TestHandler(t *testing.T) {
    store := &mockStore{
        routes: map[string]Route{
            "10.0.0.0/24": {Prefix: "10.0.0.0/24"},
        },
    }
    
    handler := NewHandler(store)
    // Test handler with mock
}
```

### Golden files

```go
func TestMarshal(t *testing.T) {
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24"}
    
    got, err := json.MarshalIndent(route, "", "  ")
    if err != nil {
        t.Fatal(err)
    }
    
    golden := filepath.Join("testdata", "route.golden")
    
    if *update {  // -update flag
        os.WriteFile(golden, got, 0644)
    }
    
    want, err := os.ReadFile(golden)
    if err != nil {
        t.Fatal(err)
    }
    
    if !bytes.Equal(got, want) {
        t.Errorf("output differs from golden file")
    }
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Shared state in parallel tests

```go
// BAD: Tests share rm
func TestBad(t *testing.T) {
    rm := NewRouteManager()
    
    t.Run("add", func(t *testing.T) {
        t.Parallel()
        rm.Add(Route{Prefix: "10.0.0.0/24"})  // Race!
    })
    
    t.Run("delete", func(t *testing.T) {
        t.Parallel()
        rm.Delete("10.0.0.0/24")  // Race!
    })
}

// GOOD: Each test has own state
func TestGood(t *testing.T) {
    t.Run("add", func(t *testing.T) {
        t.Parallel()
        rm := NewRouteManager()  // Own instance
        rm.Add(Route{Prefix: "10.0.0.0/24"})
    })
}
```

### Pitfall 2: Range variable capture

```go
// BAD: All subtests use same tt
for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        check(tt.input)  // tt is shared!
    })
}

// GOOD: Capture range variable
for _, tt := range tests {
    tt := tt  // Shadow and capture
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        check(tt.input)  // tt is local copy
    })
}
```

### Pitfall 3: Forgetting t.Helper()

```go
// BAD: Errors point to helper, not caller
func assertNil(t *testing.T, err error) {
    if err != nil {
        t.Fatalf("unexpected error: %v", err)  // Points here
    }
}

// GOOD: Errors point to caller
func assertNil(t *testing.T, err error) {
    t.Helper()
    if err != nil {
        t.Fatalf("unexpected error: %v", err)  // Points to caller
    }
}
```

---

## 6. Complete Example

```go
package route

import (
    "errors"
    "sync"
    "testing"
)

// Types and implementation
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

var ErrNotFound = errors.New("not found")
var ErrInvalid = errors.New("invalid route")

type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewRouteManager() *RouteManager {
    return &RouteManager{routes: make(map[string]Route)}
}

func (rm *RouteManager) Add(r Route) error {
    if r.Prefix == "" {
        return ErrInvalid
    }
    rm.mu.Lock()
    defer rm.mu.Unlock()
    rm.routes[r.Prefix] = r
    return nil
}

func (rm *RouteManager) Get(prefix string) (Route, error) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    r, ok := rm.routes[prefix]
    if !ok {
        return Route{}, ErrNotFound
    }
    return r, nil
}

func (rm *RouteManager) Count() int {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    return len(rm.routes)
}

// Tests
func TestRouteManager_Add(t *testing.T) {
    tests := []struct {
        name    string
        route   Route
        wantErr error
    }{
        {
            name:    "valid route",
            route:   Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
            wantErr: nil,
        },
        {
            name:    "empty prefix",
            route:   Route{VrfID: 1, Prefix: "", NextHop: "192.168.1.1"},
            wantErr: ErrInvalid,
        },
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            rm := NewRouteManager()
            err := rm.Add(tt.route)
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("Add() error = %v, wantErr %v", err, tt.wantErr)
            }
        })
    }
}

func TestRouteManager_Get(t *testing.T) {
    rm := setupWithRoutes(t, []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1"},
    })
    
    t.Run("existing route", func(t *testing.T) {
        got, err := rm.Get("10.0.0.0/24")
        assertNoError(t, err)
        assertEqual(t, got.VrfID, uint32(1))
        assertEqual(t, got.NextHop, "192.168.1.1")
    })
    
    t.Run("nonexistent route", func(t *testing.T) {
        _, err := rm.Get("192.168.0.0/24")
        if !errors.Is(err, ErrNotFound) {
            t.Errorf("expected ErrNotFound, got %v", err)
        }
    })
}

func TestRouteManager_Parallel(t *testing.T) {
    routes := []Route{
        {Prefix: "10.0.0.0/24"},
        {Prefix: "10.0.1.0/24"},
        {Prefix: "10.0.2.0/24"},
    }
    
    for _, r := range routes {
        r := r  // Capture
        t.Run(r.Prefix, func(t *testing.T) {
            t.Parallel()
            rm := NewRouteManager()
            assertNoError(t, rm.Add(r))
            got, err := rm.Get(r.Prefix)
            assertNoError(t, err)
            assertEqual(t, got.Prefix, r.Prefix)
        })
    }
}

// Test helpers
func setupWithRoutes(t *testing.T, routes []Route) *RouteManager {
    t.Helper()
    rm := NewRouteManager()
    for _, r := range routes {
        if err := rm.Add(r); err != nil {
            t.Fatalf("setup failed: %v", err)
        }
    }
    return rm
}

func assertNoError(t *testing.T, err error) {
    t.Helper()
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
}

func assertEqual[T comparable](t *testing.T, got, want T) {
    t.Helper()
    if got != want {
        t.Errorf("got %v, want %v", got, want)
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TESTING RULES                                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. TABLE-DRIVEN TESTS                                                 │
│      • One test function, many cases                                    │
│      • Easy to add new cases                                            │
│      • Clear expected vs actual                                         │
│                                                                         │
│   2. USE SUBTESTS (t.Run)                                               │
│      • Isolates test cases                                              │
│      • Enables -run filtering                                           │
│      • Better error reporting                                           │
│                                                                         │
│   3. t.Parallel() FOR INDEPENDENT TESTS                                 │
│      • Capture range variables                                          │
│      • Avoid shared mutable state                                       │
│                                                                         │
│   4. HELPERS WITH t.Helper()                                            │
│      • Marks function as helper                                         │
│      • Error points to caller                                           │
│                                                                         │
│   5. t.Cleanup() FOR RESOURCE CLEANUP                                   │
│      • Runs after test completes                                        │
│      • Even on failure                                                  │
│                                                                         │
│   6. ALWAYS RUN WITH -race                                              │
│      • go test -race ./...                                              │
│      • Catches data races                                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 测试文件约定

| 源文件 | 测试文件 |
|--------|----------|
| route.go | route_test.go |
| handler.go | handler_test.go |

### 测试函数命名

| 函数签名 | 用途 |
|----------|------|
| `TestXxx(t *testing.T)` | 单元测试 |
| `BenchmarkXxx(b *testing.B)` | 性能基准测试 |
| `ExampleXxx()` | 文档示例 |
| `FuzzXxx(f *testing.F)` | 模糊测试 |

### 表驱动测试

```go
tests := []struct {
    name    string
    input   string
    want    Route
    wantErr bool
}{
    {name: "case1", input: "...", want: ...},
    {name: "case2", input: "...", wantErr: true},
}

for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        // 测试逻辑
    })
}
```

### 最佳实践

1. **表驱动测试**：一个函数测试多种情况
2. **使用 t.Run**：隔离测试用例
3. **t.Parallel()**：并行运行独立测试
4. **t.Helper()**：标记辅助函数
5. **t.Cleanup()**：资源清理
6. **始终使用 -race**：检测数据竞争

