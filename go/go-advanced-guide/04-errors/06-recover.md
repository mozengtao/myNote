# recover: Crash Containment

## 1. Engineering Problem

### What real-world problem does this solve?

**`recover` catches panics to prevent one failure from crashing the entire program.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    RECOVER USE CASES                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   HTTP Handler:                     Worker Goroutine:                   │
│   ─────────────                     ─────────────────                   │
│                                                                         │
│   ┌─────────────┐                   ┌─────────────┐                     │
│   │  Request 1  │ ──panic──►        │  Job 1      │                     │
│   │  Request 2  │ OK                │  Job 2      │ ──panic──►          │
│   │  Request 3  │ OK                │  Job 3      │ OK                  │
│   └─────────────┘                   └─────────────┘                     │
│         │                                 │                             │
│         ▼                                 ▼                             │
│   ┌─────────────────────────┐       ┌─────────────────────────┐        │
│   │ defer + recover         │       │ defer + recover         │        │
│   │ → Return 500 error      │       │ → Log, continue pool    │        │
│   │ → Server keeps running  │       │ → Worker restarts       │        │
│   └─────────────────────────┘       └─────────────────────────┘        │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Key rules:                                                            │
│   • recover() only works in deferred function                           │
│   • recover() only catches panics in same goroutine                     │
│   • Always log the panic and stack trace                                │
│   • Convert to error, don't silently swallow                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Basic recover pattern

```go
func safeCall() (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
            log.Printf("Recovered panic:\n%s", debug.Stack())
        }
    }()
    
    riskyOperation()
    return nil
}
```

### Recover only in defer

```go
func example() {
    // BAD: recover outside defer does nothing
    if r := recover(); r != nil {
        // Never reached!
    }
    
    // GOOD: recover in defer
    defer func() {
        if r := recover(); r != nil {
            // This works
        }
    }()
    
    panic("test")
}
```

---

## 3. Language Mechanism

### recover() behavior

```go
// recover() returns:
// - nil if not in panic
// - the value passed to panic() otherwise

func example() {
    defer func() {
        r := recover()
        fmt.Printf("recovered: %v, type: %T\n", r, r)
    }()
    
    panic("error message")  // r = "error message", type string
    panic(42)               // r = 42, type int
    panic(errors.New("x"))  // r = error, type *errors.errorString
}
```

### Goroutine isolation

```go
func main() {
    defer func() {
        recover()  // Does NOT catch panics in other goroutines
    }()
    
    go func() {
        panic("goroutine panic")  // Crashes entire program!
    }()
    
    time.Sleep(time.Second)
}

// GOOD: Each goroutine needs its own recover
func main() {
    go func() {
        defer func() {
            if r := recover(); r != nil {
                log.Printf("goroutine panic: %v", r)
            }
        }()
        panic("goroutine panic")  // Recovered
    }()
    
    time.Sleep(time.Second)
}
```

---

## 4. Idiomatic Usage

### HTTP middleware

```go
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("panic: %v\n%s", err, debug.Stack())
                
                w.Header().Set("Content-Type", "application/json")
                w.WriteHeader(http.StatusInternalServerError)
                json.NewEncoder(w).Encode(map[string]string{
                    "error": "internal server error",
                })
            }
        }()
        
        next.ServeHTTP(w, r)
    })
}
```

### Worker pool

```go
func worker(id int, jobs <-chan Job, results chan<- Result) {
    for job := range jobs {
        result := safeProcess(id, job)
        results <- result
    }
}

func safeProcess(workerID int, job Job) Result {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("worker %d panic on job %v: %v\n%s",
                workerID, job.ID, r, debug.Stack())
        }
    }()
    
    return process(job)
}
```

### Library boundary

```go
// Library API: convert internal panics to errors
func (lib *Library) Execute(cmd string) (result string, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("internal error: %v", r)
        }
    }()
    
    return lib.internalExecute(cmd)
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Silently swallowing panics

```go
// BAD: Panic completely ignored
defer func() {
    recover()  // Swallowed!
}()

// GOOD: Log and handle appropriately
defer func() {
    if r := recover(); r != nil {
        log.Printf("panic: %v\n%s", r, debug.Stack())
        // Handle: return error, write response, etc.
    }
}()
```

### Pitfall 2: Recover in wrong function

```go
// BAD: recover() not directly in defer
func handleRecover() {
    if r := recover(); r != nil {
        log.Println("recovered")  // Never reached!
    }
}

func example() {
    defer handleRecover()  // Doesn't work!
    panic("test")
}

// GOOD: recover directly in anonymous defer function
func example() {
    defer func() {
        if r := recover(); r != nil {
            log.Println("recovered")  // Works!
        }
    }()
    panic("test")
}
```

### Pitfall 3: Unprotected goroutines

```go
// BAD: Goroutine panic crashes program
func serve(conn net.Conn) {
    go handleConnection(conn)  // No protection!
}

// GOOD: Protect every goroutine
func serve(conn net.Conn) {
    go func() {
        defer func() {
            if r := recover(); r != nil {
                log.Printf("connection handler panic: %v", r)
            }
        }()
        handleConnection(conn)
    }()
}
```

---

## 6. Complete Example

```go
package main

import (
    "encoding/json"
    "fmt"
    "log"
    "net/http"
    "runtime/debug"
    "sync"
    "time"
)

// Recovery middleware for HTTP
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                // Log with stack trace
                log.Printf("Panic in %s %s: %v\n%s",
                    r.Method, r.URL.Path, err, debug.Stack())
                
                // Respond with error
                w.Header().Set("Content-Type", "application/json")
                w.WriteHeader(http.StatusInternalServerError)
                json.NewEncoder(w).Encode(map[string]string{
                    "error": "internal server error",
                })
            }
        }()
        
        next.ServeHTTP(w, r)
    })
}

// Worker pool with recovery
type Job struct {
    ID   int
    Data string
}

type Result struct {
    JobID int
    Value string
    Error error
}

func startWorkerPool(workers int, jobs <-chan Job) <-chan Result {
    results := make(chan Result, workers)
    var wg sync.WaitGroup
    
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            worker(id, jobs, results)
        }(i)
    }
    
    go func() {
        wg.Wait()
        close(results)
    }()
    
    return results
}

func worker(id int, jobs <-chan Job, results chan<- Result) {
    for job := range jobs {
        result := safeProcess(id, job)
        results <- result
    }
}

func safeProcess(workerID int, job Job) Result {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("Worker %d panic on job %d: %v\n%s",
                workerID, job.ID, r, debug.Stack())
        }
    }()
    
    // Simulate processing that might panic
    if job.Data == "panic" {
        panic("simulated panic")
    }
    
    return Result{
        JobID: job.ID,
        Value: fmt.Sprintf("Processed: %s", job.Data),
    }
}

// Safe operation wrapper
func safeOperation(fn func() error) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
            log.Printf("Operation panic:\n%s", debug.Stack())
        }
    }()
    
    return fn()
}

func handler(w http.ResponseWriter, r *http.Request) {
    // This might panic
    if r.URL.Query().Get("panic") == "true" {
        panic("intentional panic for testing")
    }
    
    w.Write([]byte("OK"))
}

func main() {
    // HTTP server with recovery
    mux := http.NewServeMux()
    mux.HandleFunc("/", handler)
    
    // Wrap with recovery middleware
    server := &http.Server{
        Addr:    ":8080",
        Handler: recoveryMiddleware(mux),
    }
    
    // Worker pool example
    go func() {
        jobs := make(chan Job, 10)
        results := startWorkerPool(3, jobs)
        
        // Send jobs (including one that panics)
        go func() {
            jobs <- Job{ID: 1, Data: "normal"}
            jobs <- Job{ID: 2, Data: "panic"}  // Will panic
            jobs <- Job{ID: 3, Data: "normal"}
            close(jobs)
        }()
        
        // Collect results
        for r := range results {
            if r.Error != nil {
                log.Printf("Job %d error: %v", r.JobID, r.Error)
            } else {
                log.Printf("Job %d result: %s", r.JobID, r.Value)
            }
        }
    }()
    
    // Safe operation example
    err := safeOperation(func() error {
        panic("safe operation panic")
    })
    log.Printf("Safe operation result: %v", err)
    
    // Start server
    log.Println("Server starting on :8080")
    log.Fatal(server.ListenAndServe())
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    RECOVER RULES                                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. RECOVER ONLY IN DEFER                                              │
│      • Must be directly in defer function                               │
│      • Not in function called by defer                                  │
│                                                                         │
│   2. RECOVER ONLY CATCHES SAME GOROUTINE                                │
│      • Each goroutine needs its own recover                             │
│      • Unprotected goroutine panic = program crash                      │
│                                                                         │
│   3. ALWAYS LOG WITH STACK TRACE                                        │
│      • debug.Stack() for full trace                                     │
│      • Never silently swallow                                           │
│                                                                         │
│   4. CONVERT TO ERROR                                                   │
│      • Return error to caller                                           │
│      • HTTP: return 500 response                                        │
│                                                                         │
│   5. USE AT BOUNDARIES                                                  │
│      • HTTP handlers                                                    │
│      • Worker goroutines                                                │
│      • Library APIs                                                     │
│                                                                         │
│   6. DON'T OVERUSE                                                      │
│      • Not for control flow                                             │
│      • Only for crash containment                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### recover 核心规则

1. **只在 defer 中有效**
2. **只能捕获当前 goroutine 的 panic**
3. **返回 panic 的值**

### 使用场景

| 场景 | 说明 |
|------|------|
| HTTP 处理器 | 防止一个请求崩溃服务器 |
| Worker goroutine | 防止一个任务崩溃整个池 |
| 库边界 | 内部 panic 转换为 error |

### 常见错误

```go
// 错误：recover 不在 defer 中
recover()  // 无效！

// 错误：调用其他函数中的 recover
defer handleRecover()  // 无效！

// 正确：直接在匿名 defer 函数中
defer func() {
    if r := recover(); r != nil {
        log.Printf("panic: %v", r)
    }
}()
```

### 最佳实践

1. **总是记录日志和堆栈**
2. **转换为 error 返回**
3. **保护每个 goroutine**
4. **不要静默吞掉 panic**

