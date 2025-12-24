# panic, recover, and Crash Containment

## 1. Engineering Problem

### What real-world problem does this solve?

**panic is for truly unrecoverable situations; recover contains failures to prevent cascading crashes.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PANIC vs ERROR                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ERROR (normal flow):                                                  │
│   ────────────────────                                                  │
│   • Network timeout                 • File not found                    │
│   • Invalid input                   • Database error                    │
│   • Resource busy                   • Permission denied                 │
│                                                                         │
│   → Return error, let caller decide                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   PANIC (program cannot continue):                                      │
│   ─────────────────────────────────                                     │
│   • Nil pointer dereference         • Index out of bounds              │
│   • Failed assertion                • Invalid program state             │
│   • Bug in code                     • Unrecoverable corruption          │
│                                                                         │
│   → Crash the goroutine (or program)                                    │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   RECOVER (crash containment):                                          │
│   ─────────────────────────────                                         │
│   • HTTP handler isolation          • Worker goroutine protection       │
│   • Plugin/extension boundaries     • Library API boundaries            │
│                                                                         │
│   → Convert panic to error, continue running                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### panic unwinds the stack

```go
func main() {
    defer fmt.Println("1")
    defer fmt.Println("2")
    panic("crash!")
    fmt.Println("never reached")
}

// Output:
// 2
// 1
// panic: crash!
```

### recover catches panics

```go
func safeCall() (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
        }
    }()
    
    riskyOperation()  // May panic
    return nil
}
```

---

## 3. Language Mechanism

### panic basics

```go
// Explicit panic
panic("something went wrong")
panic(errors.New("bad state"))
panic(42)  // Any value

// Implicit panics (runtime)
var p *int
*p = 1  // nil pointer dereference

s := []int{1, 2, 3}
_ = s[10]  // index out of range
```

### recover basics

```go
func handler() {
    defer func() {
        if r := recover(); r != nil {
            // r is the value passed to panic()
            fmt.Printf("Recovered: %v\n", r)
        }
    }()
    
    panic("test")
}
```

### recover only works in defer

```go
func example() {
    // BAD: recover() returns nil outside defer
    if r := recover(); r != nil {
        // Never true!
    }
    
    panic("test")
}

func example2() {
    // GOOD: recover in defer
    defer func() {
        if r := recover(); r != nil {
            // This works
        }
    }()
    
    panic("test")
}
```

### recover only catches current goroutine

```go
func main() {
    defer func() {
        recover()  // Does NOT catch panics in other goroutines
    }()
    
    go func() {
        panic("goroutine panic")  // Crashes the program!
    }()
    
    time.Sleep(time.Second)
}
```

---

## 4. Idiomatic Usage

### HTTP handler protection

```go
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("panic: %v\nstack: %s", err, debug.Stack())
                http.Error(w, "Internal Server Error", 500)
            }
        }()
        next.ServeHTTP(w, r)
    })
}
```

### Worker goroutine protection

```go
func worker(jobs <-chan Job, results chan<- Result) {
    for job := range jobs {
        result := safeProcess(job)
        results <- result
    }
}

func safeProcess(job Job) (result Result) {
    defer func() {
        if r := recover(); r != nil {
            result = Result{Err: fmt.Errorf("panic: %v", r)}
        }
    }()
    
    return process(job)
}
```

### API boundary

```go
// Public API converts panics to errors
func (c *Client) Execute(cmd string) (result string, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("internal error: %v", r)
        }
    }()
    
    return c.internalExecute(cmd)
}
```

### Intentional panic for bugs

```go
func mustGetConfig(key string) string {
    v, ok := config[key]
    if !ok {
        panic(fmt.Sprintf("missing required config: %s", key))
    }
    return v
}

// Common pattern: MustXxx panics, Xxx returns error
func ParseIP(s string) (net.IP, error) { ... }
func MustParseIP(s string) net.IP {
    ip, err := ParseIP(s)
    if err != nil {
        panic(err)
    }
    return ip
}
```

---

## 5. Common Pitfalls

### Pitfall 1: recover in wrong place

```go
// BAD: recover() not directly in defer func
func bad() {
    defer checkRecover()  // This won't work!
    panic("test")
}

func checkRecover() {
    if r := recover(); r != nil {
        fmt.Println("recovered")  // Never prints
    }
}

// GOOD: recover() directly in defer
func good() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("recovered")
        }
    }()
    panic("test")
}
```

### Pitfall 2: Not re-panicking when appropriate

```go
// BAD: Swallowing all panics
defer func() {
    recover()  // Silently ignores everything!
}()

// GOOD: Log and potentially re-panic
defer func() {
    if r := recover(); r != nil {
        log.Printf("panic: %v\n%s", r, debug.Stack())
        // Re-panic if truly unrecoverable
        if _, ok := r.(fatalError); ok {
            panic(r)
        }
    }
}()
```

### Pitfall 3: Using panic for control flow

```go
// BAD: Using panic as "exception"
func find(items []Item, id string) Item {
    for _, item := range items {
        if item.ID == id {
            return item
        }
    }
    panic("not found")  // Don't do this!
}

// GOOD: Return error
func find(items []Item, id string) (Item, error) {
    for _, item := range items {
        if item.ID == id {
            return item, nil
        }
    }
    return Item{}, ErrNotFound
}
```

### Pitfall 4: Goroutine panics crash program

```go
// BAD: Unprotected goroutine
go func() {
    riskyOperation()  // If this panics, program crashes
}()

// GOOD: Protected goroutine
go func() {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("goroutine panic: %v", r)
        }
    }()
    riskyOperation()
}()
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "log"
    "net/http"
    "runtime/debug"
    "sync"
)

// RouteManager with panic protection
type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteManager() *RouteManager {
    return &RouteManager{
        routes: make(map[string]Route),
    }
}

// SafeAddRoute recovers from panics
func (rm *RouteManager) SafeAddRoute(r Route) (err error) {
    defer func() {
        if rec := recover(); rec != nil {
            err = fmt.Errorf("internal error: %v", rec)
            log.Printf("panic in AddRoute: %v\nStack:\n%s", rec, debug.Stack())
        }
    }()
    
    return rm.addRoute(r)
}

func (rm *RouteManager) addRoute(r Route) error {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    // Validate - might panic on bad data
    if r.Prefix == "" {
        panic("invalid route: empty prefix")  // Bug in caller
    }
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rm.routes[key] = r
    return nil
}

// Worker pool with protected workers
type WorkerPool struct {
    jobs    chan Route
    results chan error
    wg      sync.WaitGroup
}

func NewWorkerPool(workers int) *WorkerPool {
    p := &WorkerPool{
        jobs:    make(chan Route, 100),
        results: make(chan error, 100),
    }
    
    for i := 0; i < workers; i++ {
        p.wg.Add(1)
        go p.worker(i)
    }
    
    return p
}

func (p *WorkerPool) worker(id int) {
    defer p.wg.Done()
    
    for job := range p.jobs {
        err := p.safeProcess(id, job)
        p.results <- err
    }
}

func (p *WorkerPool) safeProcess(id int, r Route) (err error) {
    defer func() {
        if rec := recover(); rec != nil {
            err = fmt.Errorf("worker %d panic: %v", id, rec)
            log.Printf("Worker %d panic:\n%s", id, debug.Stack())
        }
    }()
    
    return p.process(r)
}

func (p *WorkerPool) process(r Route) error {
    // Simulate processing that might panic
    if r.VrfID == 0 {
        panic("VRF ID cannot be zero")
    }
    log.Printf("Processed route: %s", r.Prefix)
    return nil
}

func (p *WorkerPool) Submit(r Route) {
    p.jobs <- r
}

func (p *WorkerPool) Close() {
    close(p.jobs)
    p.wg.Wait()
    close(p.results)
}

// HTTP handler with recovery middleware
func recoveryMiddleware(next http.HandlerFunc) http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("HTTP panic: %v\nStack:\n%s", err, debug.Stack())
                http.Error(w, "Internal Server Error", http.StatusInternalServerError)
            }
        }()
        next(w, r)
    }
}

func routeHandler(w http.ResponseWriter, r *http.Request) {
    // Simulate potential panic
    prefix := r.URL.Query().Get("prefix")
    if prefix == "crash" {
        panic("intentional crash for testing")
    }
    fmt.Fprintf(w, "Route: %s\n", prefix)
}

// Must-style function for initialization
func MustParseRoute(s string) Route {
    r, err := ParseRoute(s)
    if err != nil {
        panic(fmt.Sprintf("invalid route %q: %v", s, err))
    }
    return r
}

func ParseRoute(s string) (Route, error) {
    // Simplified parsing
    if s == "" {
        return Route{}, fmt.Errorf("empty route string")
    }
    return Route{Prefix: s}, nil
}

func main() {
    // 1. Route manager with panic protection
    rm := NewRouteManager()
    
    err := rm.SafeAddRoute(Route{VrfID: 1, Prefix: "10.0.0.0/24"})
    if err != nil {
        log.Printf("Error: %v", err)
    }
    
    err = rm.SafeAddRoute(Route{VrfID: 1, Prefix: ""})  // Will panic internally
    if err != nil {
        log.Printf("Caught error from panic: %v", err)
    }
    
    // 2. Worker pool with protected workers
    pool := NewWorkerPool(3)
    
    pool.Submit(Route{VrfID: 1, Prefix: "10.0.0.0/24"})
    pool.Submit(Route{VrfID: 0, Prefix: "10.0.1.0/24"})  // Will panic
    pool.Submit(Route{VrfID: 2, Prefix: "10.0.2.0/24"})
    
    pool.Close()
    
    // Collect results
    for err := range pool.results {
        if err != nil {
            log.Printf("Worker error: %v", err)
        }
    }
    
    // 3. HTTP server with recovery
    http.HandleFunc("/route", recoveryMiddleware(routeHandler))
    log.Println("Server on :8080")
    // http.ListenAndServe(":8080", nil)
    
    // 4. Must-style initialization
    route := MustParseRoute("192.168.0.0/16")  // Panics if invalid
    log.Printf("Parsed: %+v", route)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PANIC/RECOVER RULES                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   WHEN TO PANIC:                                                        │
│   • Truly unrecoverable bugs                                            │
│   • Invalid program state                                               │
│   • Failed assertions (invariants)                                      │
│   • Initialization failures in Must* functions                          │
│                                                                         │
│   WHEN NOT TO PANIC:                                                    │
│   • Network/I/O errors                                                  │
│   • Invalid user input                                                  │
│   • Expected error conditions                                           │
│   • Flow control (use errors instead)                                   │
│                                                                         │
│   WHEN TO RECOVER:                                                      │
│   • HTTP handler boundaries                                             │
│   • Worker goroutine protection                                         │
│   • Library/plugin boundaries                                           │
│   • NEVER silently - always log                                         │
│                                                                         │
│   PROTECT ALL GOROUTINES:                                               │
│   • Unprotected goroutine panic = program crash                         │
│   • Use defer + recover at goroutine entry                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### panic vs error

| 场景 | 使用 |
|------|------|
| 网络超时、文件未找到、无效输入 | error |
| nil 指针、越界、程序 bug | panic |

### recover 规则

1. **只在 defer 中有效**
2. **只能捕获当前 goroutine 的 panic**
3. **返回 panic 传递的值**

### 常见用途

```go
// HTTP 处理器保护
defer func() {
    if r := recover(); r != nil {
        log.Printf("panic: %v", r)
        http.Error(w, "Error", 500)
    }
}()

// Worker goroutine 保护
go func() {
    defer func() { recover() }()
    work()
}()

// Must 风格初始化
func MustParseConfig() Config {
    c, err := ParseConfig()
    if err != nil {
        panic(err)  // 初始化失败是致命的
    }
    return c
}
```

### 最佳实践

1. **panic 用于真正不可恢复的情况**
2. **recover 要记录日志**
3. **保护所有 goroutine**
4. **不要用 panic 做流程控制**

