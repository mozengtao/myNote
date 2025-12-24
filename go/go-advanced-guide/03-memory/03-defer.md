# defer: Controlled Resource Cleanup

## 1. Engineering Problem

### What real-world problem does this solve?

**`defer` guarantees cleanup code runs when function exits, preventing resource leaks.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CLEANUP PROBLEM                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without defer (error-prone):        With defer (safe):                │
│   ────────────────────────────        ──────────────────                │
│                                                                         │
│   func process() error {              func process() error {            │
│       f, err := os.Open(...)              f, err := os.Open(...)        │
│       if err != nil {                     if err != nil {               │
│           return err                          return err                │
│       }                                   }                             │
│       // forget to close!                 defer f.Close()  // Always!   │
│                                                                         │
│       data, err := read(f)                data, err := read(f)          │
│       if err != nil {                     if err != nil {               │
│           f.Close()  // Duplicate             return err  // Close runs │
│           return err                      }                             │
│       }                                                                 │
│                                           return process(data)          │
│       result := process(data)         }                                 │
│       f.Close()  // Easy to miss                                        │
│       return result                                                     │
│   }                                                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Defer executes:                                                       │
│   • When function returns (normal or panic)                             │
│   • In LIFO order (last defer first)                                    │
│   • After return value is set                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### LIFO execution order

```go
func example() {
    defer fmt.Println("first")   // Executes 3rd
    defer fmt.Println("second")  // Executes 2nd
    defer fmt.Println("third")   // Executes 1st
    
    fmt.Println("main")
}

// Output:
// main
// third
// second
// first
```

### Arguments evaluated immediately

```go
func example() {
    x := 1
    defer fmt.Println(x)  // x=1 captured NOW
    x = 2
    fmt.Println(x)
}

// Output:
// 2
// 1  (not 2!)
```

---

## 3. Language Mechanism

### Basic defer

```go
func readFile(path string) ([]byte, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close()  // Guaranteed to run
    
    return io.ReadAll(f)
}
```

### Defer with closures

```go
func example() {
    x := 1
    defer func() {
        fmt.Println(x)  // Captures x by reference
    }()
    x = 2
}
// Output: 2 (closure sees final value)
```

### Named return values

```go
func example() (result int) {
    defer func() {
        result++  // Can modify return value!
    }()
    return 1  // Returns 2, not 1
}
```

### Defer in loops

```go
// BAD: Defers accumulate, all run at function end
func processFiles(paths []string) error {
    for _, path := range paths {
        f, err := os.Open(path)
        if err != nil {
            return err
        }
        defer f.Close()  // All deferred until function returns!
    }
    return nil
}

// GOOD: Use helper function or explicit close
func processFiles(paths []string) error {
    for _, path := range paths {
        if err := processOneFile(path); err != nil {
            return err
        }
    }
    return nil
}

func processOneFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()
    // Process file
    return nil
}
```

---

## 4. Idiomatic Usage

### Pattern 1: Mutex unlock

```go
func (rm *RouteManager) AddRoute(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    // Safe: unlock runs even if panic
    rm.routes[r.Key()] = r
}
```

### Pattern 2: Cleanup pairs

```go
func withTempFile(fn func(*os.File) error) error {
    f, err := os.CreateTemp("", "route-*")
    if err != nil {
        return err
    }
    defer os.Remove(f.Name())  // Cleanup temp file
    defer f.Close()            // Close first (LIFO)
    
    return fn(f)
}
```

### Pattern 3: Timing/tracing

```go
func (rm *RouteManager) AddRoute(r Route) error {
    start := time.Now()
    defer func() {
        log.Printf("AddRoute took %v", time.Since(start))
    }()
    
    // ... work ...
    return nil
}
```

### Pattern 4: Recover from panic

```go
func safeHandler(w http.ResponseWriter, r *http.Request) {
    defer func() {
        if err := recover(); err != nil {
            log.Printf("panic: %v", err)
            http.Error(w, "Internal Error", 500)
        }
    }()
    
    // Handler code that might panic
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Ignoring Close error

```go
// BAD: Error ignored
defer f.Close()

// GOOD: Check error with named return
func writeFile(path string, data []byte) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    defer func() {
        if cerr := f.Close(); cerr != nil && err == nil {
            err = cerr
        }
    }()
    
    _, err = f.Write(data)
    return err
}
```

### Pitfall 2: Defer in loop

```go
// BAD: File descriptors exhausted
func processAll(paths []string) error {
    for _, path := range paths {
        f, _ := os.Open(path)
        defer f.Close()  // Not closed until function returns!
    }
    return nil
}

// GOOD: Close in each iteration
for _, path := range paths {
    func() {
        f, _ := os.Open(path)
        defer f.Close()
        // process
    }()
}
```

### Pitfall 3: Nil receiver in defer

```go
// BUG: Panic when mu is nil
func process(mu *sync.Mutex) {
    mu.Lock()
    defer mu.Unlock()  // Panics if mu is nil!
}

// FIX: Check before defer
func process(mu *sync.Mutex) {
    if mu != nil {
        mu.Lock()
        defer mu.Unlock()
    }
}
```

### Pitfall 4: Performance in tight loops

```go
// BAD: defer has overhead (~35ns)
func sum(nums []int) int {
    total := 0
    for _, n := range nums {
        func() {
            defer func() {}()  // Overhead per iteration!
            total += n
        }()
    }
    return total
}

// GOOD: Don't use defer in tight loops
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "fmt"
    "os"
    "sync"
    "time"
)

type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
    log    *os.File
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteManager(logPath string) (*RouteManager, error) {
    f, err := os.OpenFile(logPath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
    if err != nil {
        return nil, fmt.Errorf("open log: %w", err)
    }
    
    return &RouteManager{
        routes: make(map[string]Route),
        log:    f,
    }, nil
}

// Close cleans up resources
func (rm *RouteManager) Close() error {
    return rm.log.Close()
}

// AddRoute with timing and proper locking
func (rm *RouteManager) AddRoute(r Route) (err error) {
    start := time.Now()
    defer func() {
        duration := time.Since(start)
        status := "success"
        if err != nil {
            status = "failed"
        }
        fmt.Fprintf(rm.log, "AddRoute %s:%s %s (%v)\n",
            r.Prefix, r.NextHop, status, duration)
    }()
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
    return nil
}

// GetRoute with read lock
func (rm *RouteManager) GetRoute(vrfID uint32, prefix string) (Route, bool) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rm.routes[key]
    return r, ok
}

// ProcessWithTimeout uses context for timeout
func (rm *RouteManager) ProcessWithTimeout(ctx context.Context) error {
    done := make(chan struct{})
    defer close(done)  // Signal completion
    
    go func() {
        select {
        case <-ctx.Done():
            fmt.Println("Processing cancelled")
        case <-done:
            fmt.Println("Processing completed")
        }
    }()
    
    // Simulate work
    time.Sleep(100 * time.Millisecond)
    return nil
}

// SafeOperation recovers from panics
func (rm *RouteManager) SafeOperation(fn func() error) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
        }
    }()
    
    return fn()
}

func main() {
    rm, err := NewRouteManager("/tmp/routes.log")
    if err != nil {
        fmt.Println("Error:", err)
        return
    }
    defer rm.Close()  // Cleanup on exit
    
    // Add routes
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
    }
    
    for _, r := range routes {
        if err := rm.AddRoute(r); err != nil {
            fmt.Println("Error:", err)
        }
    }
    
    // Query
    if r, ok := rm.GetRoute(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", r)
    }
    
    // Safe operation that might panic
    err = rm.SafeOperation(func() error {
        panic("test panic")
    })
    fmt.Println("Recovered error:", err)
    
    // Timeout operation
    ctx, cancel := context.WithTimeout(context.Background(), time.Second)
    defer cancel()
    rm.ProcessWithTimeout(ctx)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DEFER RULES                                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ALWAYS DEFER CLEANUP                                               │
│      • Mutex unlock                                                     │
│      • File/connection close                                            │
│      • Channel close                                                    │
│                                                                         │
│   2. REMEMBER LIFO ORDER                                                │
│      • Last defer runs first                                            │
│      • Cleanup in reverse order of acquisition                          │
│                                                                         │
│   3. ARGUMENTS EVALUATED IMMEDIATELY                                    │
│      • Use closure to capture current values                            │
│      • Or pass values explicitly                                        │
│                                                                         │
│   4. HANDLE CLOSE ERRORS                                                │
│      • Use named returns                                                │
│      • Check error in defer closure                                     │
│                                                                         │
│   5. AVOID DEFER IN LOOPS                                               │
│      • Use helper functions                                             │
│      • Or explicit close in each iteration                              │
│                                                                         │
│   6. DEFER HAS SMALL OVERHEAD                                           │
│      • ~35ns per defer                                                  │
│      • Avoid in tight loops                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### defer 核心概念

**`defer` 保证函数退出时执行清理代码，防止资源泄漏。**

### 执行顺序

- LIFO（后进先出）：最后的 defer 最先执行
- 在函数返回后、返回调用者前执行
- panic 时也会执行

### 参数求值时机

```go
x := 1
defer fmt.Println(x)  // x=1 立即求值
x = 2
// 输出: 1
```

### 常见用途

1. **解锁互斥锁**：`defer mu.Unlock()`
2. **关闭文件**：`defer f.Close()`
3. **关闭连接**：`defer conn.Close()`
4. **计时/追踪**：记录函数执行时间
5. **panic 恢复**：`defer func() { recover() }()`

### 常见陷阱

1. **循环中的 defer**：所有 defer 在函数结束才执行
2. **忽略 Close 错误**：用命名返回值处理
3. **nil 接收器**：可能 panic
4. **性能开销**：紧密循环中避免使用

### 最佳实践

- 资源获取后立即 defer 清理
- 使用命名返回值处理 Close 错误
- 循环中用辅助函数

