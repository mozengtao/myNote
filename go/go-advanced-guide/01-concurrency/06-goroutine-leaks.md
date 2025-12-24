# Goroutine Leaks: Detection and Prevention

## 1. Engineering Problem

### What real-world problem does this solve?

**A goroutine leak is a goroutine that runs forever without doing useful work, consuming memory indefinitely.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE GOROUTINE LEAK PROBLEM                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Each goroutine costs:                                                 │
│   • Minimum 2KB stack (can grow to MBs)                                │
│   • Scheduler overhead                                                  │
│   • Potential reference retention (prevents GC)                         │
│                                                                         │
│   Over time:                                                            │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │                                                               │     │
│   │   Goroutines                                                  │     │
│   │       ▲                                          ╱            │     │
│   │       │                                        ╱              │     │
│   │       │                                      ╱                │     │
│   │       │                                    ╱                  │     │
│   │       │                                 ╱                     │     │
│   │       │                              ╱                        │     │
│   │       │                           ╱                           │     │
│   │       │                        ╱  ◄── Leaked goroutines      │     │
│   │       │                     ╱                                 │     │
│   │       │                  ╱                                    │     │
│   │       │───────────────╱                                       │     │
│   │       │ Expected level                                        │     │
│   │       └───────────────────────────────────────────────► time  │     │
│   │                                                               │     │
│   └───────────────────────────────────────────────────────────────┘     │
│                                                                         │
│   Result: Memory exhaustion, OOM kill, service degradation              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Common leak scenarios

1. **Blocked on channel send**: No receiver ever comes
2. **Blocked on channel receive**: Channel never closed
3. **Blocked on mutex**: Deadlock or very slow lock holder
4. **Blocked on I/O**: Network call with no timeout
5. **Infinite loop**: No exit condition checked

---

## 2. Core Mental Model

### What makes a leak?

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LEAK CLASSIFICATION                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   NOT A LEAK:                                                           │
│   ───────────                                                           │
│   • Goroutine that's doing work and will eventually finish              │
│   • Goroutine waiting for legitimate condition (with timeout)           │
│   • Goroutine in pool waiting for next job (with shutdown path)         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   IS A LEAK:                                                            │
│   ──────────                                                            │
│   • Goroutine waiting on channel that will NEVER be ready               │
│   • Goroutine in loop with no exit condition that applies               │
│   • Goroutine blocked on I/O with no timeout/cancellation               │
│   • Goroutine whose purpose is complete but hasn't returned             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Key Question: Is there a condition under which this goroutine         │
│                 will DEFINITELY return?                                 │
│                                                                         │
│   If no → LEAK                                                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Common Leak Patterns

### Pattern 1: Abandoned channel sender

```go
// LEAK: If nobody reads from ch, this goroutine blocks forever
func leak1() {
    ch := make(chan int)
    go func() {
        ch <- 42  // Blocks forever
    }()
    // ch goes out of scope, but goroutine still exists
}

// FIX: Use buffered channel or ensure receiver exists
func fixed1(ctx context.Context) {
    ch := make(chan int, 1)  // Buffered: won't block
    go func() {
        ch <- 42  // Completes immediately
    }()
}

// OR: Use select with cancellation
func fixed1b(ctx context.Context) {
    ch := make(chan int)
    go func() {
        select {
        case ch <- 42:
        case <-ctx.Done():
            return  // Exit if context cancelled
        }
    }()
}
```

### Pattern 2: Abandoned channel receiver

```go
// LEAK: If ch never receives or closes, goroutine blocks forever
func leak2() {
    ch := make(chan int)
    go func() {
        for v := range ch {  // Waits for close forever
            process(v)
        }
    }()
    // Forgot to close(ch)
}

// FIX: Always close channels, or use ctx.Done()
func fixed2(ctx context.Context) {
    ch := make(chan int)
    go func() {
        defer close(ch)
        // Send data...
    }()
    
    go func() {
        for {
            select {
            case <-ctx.Done():
                return
            case v, ok := <-ch:
                if !ok {
                    return
                }
                process(v)
            }
        }
    }()
}
```

### Pattern 3: Goroutine waiting on itself

```go
// LEAK: Classic deadlock
func leak3() {
    ch := make(chan int)
    go func() {
        ch <- <-ch  // Sends what it receives... from itself
    }()
}
```

### Pattern 4: No timeout on network I/O

```go
// LEAK: If server never responds, blocked forever
func leak4(url string) {
    go func() {
        resp, _ := http.Get(url)  // No timeout!
        _ = resp
    }()
}

// FIX: Use client with timeout
func fixed4(ctx context.Context, url string) {
    go func() {
        client := &http.Client{Timeout: 10 * time.Second}
        req, _ := http.NewRequestWithContext(ctx, "GET", url, nil)
        resp, err := client.Do(req)
        if err != nil {
            return
        }
        defer resp.Body.Close()
        // Process response...
    }()
}
```

### Pattern 5: Worker pool without shutdown

```go
// LEAK: Workers never stop
func leak5() {
    jobs := make(chan Job)
    
    for i := 0; i < 10; i++ {
        go func() {
            for job := range jobs {  // Waits forever
                process(job)
            }
        }()
    }
    
    // Never close(jobs)
}

// FIX: Proper shutdown
func fixed5(ctx context.Context) {
    jobs := make(chan Job)
    var wg sync.WaitGroup
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for {
                select {
                case <-ctx.Done():
                    return
                case job, ok := <-jobs:
                    if !ok {
                        return
                    }
                    process(job)
                }
            }
        }()
    }
    
    // On shutdown:
    // close(jobs)
    // wg.Wait()
}
```

### Pattern 6: Conditional leak (hard to find)

```go
// LEAK: Only leaks under error condition
func leak6() {
    resultCh := make(chan Result)
    
    go func() {
        result, err := doExpensiveWork()
        if err != nil {
            return  // Result channel never receives
        }
        resultCh <- result
    }()
    
    <-resultCh  // Blocks forever on error path
}

// FIX: Always send something or use context
func fixed6(ctx context.Context) (Result, error) {
    resultCh := make(chan Result, 1)
    errCh := make(chan error, 1)
    
    go func() {
        result, err := doExpensiveWork()
        if err != nil {
            errCh <- err
            return
        }
        resultCh <- result
    }()
    
    select {
    case <-ctx.Done():
        return Result{}, ctx.Err()
    case err := <-errCh:
        return Result{}, err
    case result := <-resultCh:
        return result, nil
    }
}
```

---

## 4. Detection Techniques

### Technique 1: Monitor runtime.NumGoroutine()

```go
import "runtime"

func monitorGoroutines() {
    ticker := time.NewTicker(10 * time.Second)
    var lastCount int
    
    for range ticker.C {
        count := runtime.NumGoroutine()
        if count > lastCount+100 {
            log.Printf("WARNING: Goroutine count increased by %d to %d",
                count-lastCount, count)
        }
        lastCount = count
    }
}
```

### Technique 2: Use runtime/pprof

```go
import "runtime/pprof"

// Dump all goroutine stacks
func dumpGoroutines() {
    pprof.Lookup("goroutine").WriteTo(os.Stdout, 1)
}

// HTTP endpoint for debugging
import _ "net/http/pprof"
// Access: http://localhost:6060/debug/pprof/goroutine?debug=1
```

### Technique 3: goleak in tests

```go
import "go.uber.org/goleak"

func TestMain(m *testing.M) {
    goleak.VerifyTestMain(m)
}

func TestSomething(t *testing.T) {
    defer goleak.VerifyNone(t)
    
    // Test code...
}
```

### Technique 4: Track goroutine lifecycle

```go
type GoroutineTracker struct {
    active sync.WaitGroup
    count  int64
}

func (gt *GoroutineTracker) Go(f func()) {
    gt.active.Add(1)
    atomic.AddInt64(&gt.count, 1)
    
    go func() {
        defer gt.active.Done()
        defer atomic.AddInt64(&gt.count, -1)
        f()
    }()
}

func (gt *GoroutineTracker) Wait() {
    gt.active.Wait()
}

func (gt *GoroutineTracker) Count() int64 {
    return atomic.LoadInt64(&gt.count)
}
```

---

## 5. Complete Example: Leak-proof HTTP client

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "io"
    "log"
    "net/http"
    "runtime"
    "sync"
    "sync/atomic"
    "time"
)

// SafeHTTPClient wraps http.Client with leak prevention
type SafeHTTPClient struct {
    client      *http.Client
    maxInFlight int64
    inFlight    int64
    
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
}

func NewSafeHTTPClient(timeout time.Duration, maxInFlight int) *SafeHTTPClient {
    ctx, cancel := context.WithCancel(context.Background())
    
    return &SafeHTTPClient{
        client: &http.Client{
            Timeout: timeout,
            Transport: &http.Transport{
                MaxIdleConns:        100,
                IdleConnTimeout:     90 * time.Second,
                DisableCompression:  false,
                DisableKeepAlives:   false,
            },
        },
        maxInFlight: int64(maxInFlight),
        ctx:         ctx,
        cancel:      cancel,
    }
}

// Get performs HTTP GET with proper goroutine management
func (c *SafeHTTPClient) Get(ctx context.Context, url string) ([]byte, error) {
    // Check if shutdown in progress
    select {
    case <-c.ctx.Done():
        return nil, errors.New("client is shut down")
    default:
    }
    
    // Check in-flight limit
    if atomic.LoadInt64(&c.inFlight) >= c.maxInFlight {
        return nil, errors.New("too many in-flight requests")
    }
    
    // Track this request
    atomic.AddInt64(&c.inFlight, 1)
    c.wg.Add(1)
    
    // Use channel for result
    type result struct {
        data []byte
        err  error
    }
    resultCh := make(chan result, 1)
    
    go func() {
        defer c.wg.Done()
        defer atomic.AddInt64(&c.inFlight, -1)
        
        // Create request with combined context
        reqCtx, cancel := context.WithCancel(ctx)
        defer cancel()
        
        // Also respect client-level context
        go func() {
            select {
            case <-c.ctx.Done():
                cancel()
            case <-reqCtx.Done():
            }
        }()
        
        req, err := http.NewRequestWithContext(reqCtx, "GET", url, nil)
        if err != nil {
            resultCh <- result{nil, err}
            return
        }
        
        resp, err := c.client.Do(req)
        if err != nil {
            resultCh <- result{nil, err}
            return
        }
        defer resp.Body.Close()
        
        // Read with limit to prevent memory exhaustion
        limitedReader := io.LimitReader(resp.Body, 10*1024*1024) // 10MB max
        data, err := io.ReadAll(limitedReader)
        resultCh <- result{data, err}
    }()
    
    // Wait for result or cancellation
    select {
    case <-c.ctx.Done():
        return nil, errors.New("client shutdown")
    case <-ctx.Done():
        return nil, ctx.Err()
    case res := <-resultCh:
        return res.data, res.err
    }
}

// GetAsync performs non-blocking HTTP GET
func (c *SafeHTTPClient) GetAsync(ctx context.Context, url string, callback func([]byte, error)) {
    c.wg.Add(1)
    
    go func() {
        defer c.wg.Done()
        
        data, err := c.Get(ctx, url)
        
        // Deliver callback with cancellation check
        select {
        case <-c.ctx.Done():
            return  // Don't deliver callback during shutdown
        default:
            callback(data, err)
        }
    }()
}

// InFlight returns current in-flight request count
func (c *SafeHTTPClient) InFlight() int64 {
    return atomic.LoadInt64(&c.inFlight)
}

// Shutdown gracefully shuts down the client
func (c *SafeHTTPClient) Shutdown(timeout time.Duration) error {
    c.cancel()  // Signal all goroutines
    
    done := make(chan struct{})
    go func() {
        c.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        return nil
    case <-time.After(timeout):
        return fmt.Errorf("shutdown timeout: %d requests still in flight",
            atomic.LoadInt64(&c.inFlight))
    }
}

// LeakDetector helps find goroutine leaks
type LeakDetector struct {
    baseline int
    name     string
}

func NewLeakDetector(name string) *LeakDetector {
    return &LeakDetector{
        baseline: runtime.NumGoroutine(),
        name:     name,
    }
}

func (ld *LeakDetector) Check() error {
    // Allow for some variance and background goroutines
    current := runtime.NumGoroutine()
    delta := current - ld.baseline
    
    if delta > 5 {  // Threshold
        return fmt.Errorf("%s: potential leak - goroutines increased by %d (baseline: %d, current: %d)",
            ld.name, delta, ld.baseline, current)
    }
    return nil
}

func main() {
    fmt.Println("=== Goroutine Leak Prevention Demo ===")
    
    // Monitor goroutine count
    go func() {
        ticker := time.NewTicker(time.Second)
        for range ticker.C {
            log.Printf("Goroutines: %d", runtime.NumGoroutine())
        }
    }()
    
    detector := NewLeakDetector("main")
    
    // Create safe client
    client := NewSafeHTTPClient(5*time.Second, 10)
    
    // Simulate requests
    ctx := context.Background()
    
    for i := 0; i < 5; i++ {
        go func(id int) {
            reqCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
            defer cancel()
            
            data, err := client.Get(reqCtx, "https://httpbin.org/delay/1")
            if err != nil {
                log.Printf("Request %d failed: %v", id, err)
            } else {
                log.Printf("Request %d: received %d bytes", id, len(data))
            }
        }(i)
    }
    
    // Wait for requests
    time.Sleep(5 * time.Second)
    
    log.Printf("In-flight requests: %d", client.InFlight())
    
    // Shutdown
    if err := client.Shutdown(10 * time.Second); err != nil {
        log.Printf("Shutdown error: %v", err)
    }
    
    // Check for leaks
    time.Sleep(100 * time.Millisecond)  // Allow cleanup
    if err := detector.Check(); err != nil {
        log.Printf("LEAK DETECTED: %v", err)
    } else {
        log.Println("No leaks detected")
    }
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 6. Design Takeaways

### Prevention Checklist

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 GOROUTINE LEAK PREVENTION CHECKLIST                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   For every goroutine:                                                  │
│                                                                         │
│   □ Does it have a guaranteed exit condition?                           │
│   □ Does every blocking operation have a timeout/context?               │
│   □ Are channels properly managed (closed when done)?                   │
│   □ Is there a select with ctx.Done() in every loop?                    │
│   □ Are all send operations protected with select or buffer?            │
│                                                                         │
│   For every channel:                                                    │
│                                                                         │
│   □ Is there exactly one closer (the sender)?                           │
│   □ Can receivers be sure they won't block forever?                     │
│   □ Is buffer size appropriate for the use case?                        │
│                                                                         │
│   For testing:                                                          │
│                                                                         │
│   □ Use goleak in test suite                                            │
│   □ Test shutdown paths                                                 │
│   □ Test error conditions (partial failures)                            │
│   □ Monitor runtime.NumGoroutine() in production                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Goroutine 泄漏定义

**Goroutine 泄漏是指永远运行但不做有用工作的 goroutine，持续消耗内存。**

### 常见泄漏模式

| 模式 | 原因 | 解决方案 |
|------|------|----------|
| Channel 发送阻塞 | 没有接收者 | 使用缓冲 channel 或 select+ctx |
| Channel 接收阻塞 | Channel 从不关闭 | 确保关闭或使用 ctx.Done() |
| 网络 I/O 阻塞 | 无超时设置 | 使用带超时的客户端和 context |
| Worker 池阻塞 | 没有关闭机制 | 正确的 shutdown 流程 |
| 条件泄漏 | 仅在错误路径泄漏 | 所有路径都必须处理 |

### 检测技术

1. **监控 `runtime.NumGoroutine()`**
2. **使用 `pprof` 分析 goroutine 堆栈**
3. **在测试中使用 `goleak`**
4. **跟踪 goroutine 生命周期**

### 预防原则

1. **每个 goroutine 必须有保证的退出条件**
2. **每个阻塞操作必须有超时或 context**
3. **Channel 必须正确管理（发送者关闭）**
4. **每个循环中的 select 必须包含 ctx.Done()**
5. **所有发送操作用 select 或缓冲保护**

