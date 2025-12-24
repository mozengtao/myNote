# defer: Controlled RAII in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**`defer` guarantees cleanup code runs, regardless of how a function exits.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DEFER AS RAII                                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   C++ RAII:                         Go defer:                           │
│   ─────────                         ─────────                           │
│                                                                         │
│   class Lock {                      func process() error {              │
│     Mutex& m;                           mu.Lock()                       │
│   public:                               defer mu.Unlock()               │
│     Lock(Mutex& m): m(m) {                                              │
│       m.lock();                         // ... work ...                 │
│     }                                   // Unlock runs automatically    │
│     ~Lock() {                       }                                   │
│       m.unlock();                                                       │
│     }                                                                   │
│   };                                                                    │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   defer execution order (LIFO):                                         │
│                                                                         │
│   func example() {                                                      │
│       defer fmt.Println("3")  // Third to defer, first to execute      │
│       defer fmt.Println("2")  // Second to defer, second to execute    │
│       defer fmt.Println("1")  // First to defer, third to execute      │
│   }                                                                     │
│   // Output: 3, 2, 1                                                    │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Key properties:                                                       │
│   • Executes when function returns (normal or panic)                    │
│   • Arguments evaluated when defer is reached                           │
│   • LIFO order (last in, first out)                                     │
│   • Can modify named return values                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Defer stack

```go
func example() (result string) {
    defer func() { result = "modified" }()  // Can modify result
    
    defer cleanup3()  // Runs third-to-last
    defer cleanup2()  // Runs second-to-last
    defer cleanup1()  // Runs last
    
    return "original"  // result becomes "original", then defer runs
    // Final result: "modified"
}
```

### Arguments captured at defer time

```go
func example() {
    x := 1
    defer fmt.Println("x =", x)  // Captures x=1 NOW
    x = 100
}
// Output: x = 1 (not 100!)

// To capture final value, use closure:
func example2() {
    x := 1
    defer func() { fmt.Println("x =", x) }()  // Captures reference
    x = 100
}
// Output: x = 100
```

---

## 3. Language Mechanism

### Basic defer patterns

```go
// Mutex unlock
func (rm *RouteManager) UpdateRoute(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()  // Always unlocks
    
    rm.routes[r.Key()] = r
}

// File close
func readConfig(path string) (Config, error) {
    f, err := os.Open(path)
    if err != nil {
        return Config{}, err
    }
    defer f.Close()  // Always closes
    
    return parseConfig(f)
}

// Connection cleanup
func query(db *sql.DB) error {
    tx, err := db.Begin()
    if err != nil {
        return err
    }
    defer tx.Rollback()  // Rollback if not committed
    
    // ... queries ...
    
    return tx.Commit()  // Commit succeeds, Rollback is no-op
}
```

### Defer with named returns

```go
func writeFile(path string, data []byte) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    
    defer func() {
        closeErr := f.Close()
        if err == nil {  // Only override if write succeeded
            err = closeErr
        }
    }()
    
    _, err = f.Write(data)
    return err  // If Write fails, err is set; Close error handled in defer
}
```

### Recover in defer

```go
func safeHandler(w http.ResponseWriter, r *http.Request) {
    defer func() {
        if rec := recover(); rec != nil {
            log.Printf("panic: %v", rec)
            http.Error(w, "Internal Error", 500)
        }
    }()
    
    handleRequest(w, r)  // May panic
}
```

---

## 4. Idiomatic Usage

### Resource cleanup pairs

```go
// Acquire → defer Release
mu.Lock()
defer mu.Unlock()

// Open → defer Close
f, _ := os.Open(path)
defer f.Close()

// Begin → defer Rollback
tx, _ := db.Begin()
defer tx.Rollback()

// Create → defer Remove (temp files)
f, _ := os.CreateTemp("", "temp")
defer os.Remove(f.Name())
defer f.Close()
```

### Timing and tracing

```go
func slowOperation() {
    defer trace("slowOperation")()  // Note: ()
    // ... work ...
}

func trace(name string) func() {
    start := time.Now()
    log.Printf("%s: start", name)
    return func() {
        log.Printf("%s: done (%v)", name, time.Since(start))
    }
}
```

### Context cancellation

```go
func process(ctx context.Context) error {
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()  // Always cancel to free resources
    
    return doWork(ctx)
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Defer in loop

```go
// BAD: All defers accumulate, run at function end
func processFiles(paths []string) error {
    for _, path := range paths {
        f, _ := os.Open(path)
        defer f.Close()  // All files stay open until function returns!
    }
    return nil
}

// GOOD: Use helper function
func processFiles(paths []string) error {
    for _, path := range paths {
        if err := processOneFile(path); err != nil {
            return err
        }
    }
    return nil
}

func processOneFile(path string) error {
    f, _ := os.Open(path)
    defer f.Close()  // Closes after each file
    return process(f)
}
```

### Pitfall 2: Ignoring defer'd Close error

```go
// BAD: Close error ignored for writes
func write(path string, data []byte) error {
    f, _ := os.Create(path)
    defer f.Close()  // Error ignored!
    _, err := f.Write(data)
    return err
}

// GOOD: Handle Close error
func write(path string, data []byte) (err error) {
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

### Pitfall 3: Defer performance in tight loops

```go
// BAD: Defer overhead in tight loop (~35ns per defer)
func sum(nums []int) int {
    total := 0
    for _, n := range nums {
        func() {
            defer func() {}()  // Overhead per iteration
            total += n
        }()
    }
    return total
}

// GOOD: Avoid defer in hot path
func sum(nums []int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "fmt"
    "log"
    "os"
    "sync"
    "time"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
    log    *os.File
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

// Close releases resources
func (rm *RouteManager) Close() error {
    return rm.log.Close()
}

// AddRoute with proper locking
func (rm *RouteManager) AddRoute(r Route) error {
    rm.mu.Lock()
    defer rm.mu.Unlock()  // Always unlocks
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
    
    // Log the addition
    _, err := fmt.Fprintf(rm.log, "ADD %s\n", key)
    return err
}

// GetRoute with read lock
func (rm *RouteManager) GetRoute(vrfID uint32, prefix string) (Route, bool) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rm.routes[key]
    return r, ok
}

// ProcessWithTimeout demonstrates context cancel
func (rm *RouteManager) ProcessWithTimeout(ctx context.Context, r Route) error {
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()  // Always cancel to release resources
    
    done := make(chan error, 1)
    go func() {
        done <- rm.AddRoute(r)
    }()
    
    select {
    case err := <-done:
        return err
    case <-ctx.Done():
        return ctx.Err()
    }
}

// WriteRoutes demonstrates defer with error handling
func (rm *RouteManager) WriteRoutes(path string) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    
    // Handle Close error
    defer func() {
        if cerr := f.Close(); cerr != nil && err == nil {
            err = fmt.Errorf("close: %w", cerr)
        }
    }()
    
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    for key, r := range rm.routes {
        if _, err := fmt.Fprintf(f, "%s: %+v\n", key, r); err != nil {
            return fmt.Errorf("write: %w", err)
        }
    }
    
    return nil
}

// trace demonstrates defer for timing
func trace(name string) func() {
    start := time.Now()
    log.Printf("[%s] start", name)
    return func() {
        log.Printf("[%s] done (%v)", name, time.Since(start))
    }
}

// SafeOperation demonstrates panic recovery
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
        log.Fatal(err)
    }
    defer rm.Close()  // Cleanup on exit
    
    // Add routes with timing
    defer trace("main")()
    
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
    }
    
    for _, r := range routes {
        if err := rm.AddRoute(r); err != nil {
            log.Printf("Error: %v", err)
        }
    }
    
    // Query
    if r, ok := rm.GetRoute(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", r)
    }
    
    // Write with error handling
    if err := rm.WriteRoutes("/tmp/routes.txt"); err != nil {
        log.Printf("Write error: %v", err)
    }
    
    // Safe operation
    err = rm.SafeOperation(func() error {
        panic("test panic")
    })
    log.Printf("Recovered: %v", err)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DEFER RULES                                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE DEFER FOR CLEANUP                                              │
│      • Mutex unlock                                                     │
│      • File/connection close                                            │
│      • Context cancel                                                   │
│                                                                         │
│   2. REMEMBER LIFO ORDER                                                │
│      • Last defer runs first                                            │
│      • Acquire in order, release in reverse                             │
│                                                                         │
│   3. ARGUMENTS EVALUATED AT DEFER TIME                                  │
│      • Use closure to capture final value                               │
│                                                                         │
│   4. AVOID DEFER IN LOOPS                                               │
│      • Use helper function instead                                      │
│      • Or explicit cleanup per iteration                                │
│                                                                         │
│   5. HANDLE CLOSE ERRORS FOR WRITES                                     │
│      • Use named returns                                                │
│      • Check error in defer closure                                     │
│                                                                         │
│   6. DEFER HAS SMALL OVERHEAD (~35ns)                                   │
│      • Avoid in very hot loops                                          │
│      • Fine for normal code                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### defer 核心概念

**defer 保证函数退出时执行清理代码，无论正常返回还是 panic。**

### 执行规则

- **LIFO 顺序**：后 defer 的先执行
- **参数立即求值**：defer 时求值，非执行时
- **可修改命名返回值**：defer 闭包可以修改

### 常见用法

```go
// 锁
mu.Lock()
defer mu.Unlock()

// 文件
f, _ := os.Open(path)
defer f.Close()

// 上下文
ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
defer cancel()

// panic 恢复
defer func() {
    if r := recover(); r != nil {
        log.Printf("panic: %v", r)
    }
}()
```

### 常见陷阱

1. **循环中的 defer**：累积到函数结束才执行
2. **忽略 Close 错误**：写入时必须处理
3. **热循环中的开销**：~35ns/次

### 最佳实践

- 用于资源清理配对
- 记住 LIFO 顺序
- 用闭包捕获最终值
- 写入操作处理 Close 错误

