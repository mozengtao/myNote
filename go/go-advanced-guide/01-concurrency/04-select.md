# Select Statement: Channel Multiplexing

## 1. Engineering Problem

### What real-world problem does this solve?

**The fundamental challenge: A goroutine needs to wait on multiple channels simultaneously.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE MULTIPLEXING PROBLEM                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without select:                                                       │
│   ────────────────                                                      │
│                                                                         │
│   v1 := <-ch1   // Blocks here, can't respond to ch2 or timeout        │
│   v2 := <-ch2   // Only runs after ch1 completes                        │
│                                                                         │
│   Problem: Sequential waiting, no timeout, no cancellation              │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   With select:                                                          │
│   ─────────────                                                         │
│                                                                         │
│   select {                                                              │
│   case v1 := <-ch1:    ◄─── Any of these                                │
│   case v2 := <-ch2:    ◄─── can be selected                             │
│   case <-timeout:      ◄─── if ready                                    │
│   case <-ctx.Done():   ◄─── first                                       │
│   }                                                                     │
│                                                                         │
│   Solution: Wait on any, react to whichever is ready first              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Real scenarios requiring select:**
- HTTP handler waiting for response OR context cancellation
- Worker receiving jobs OR shutdown signal
- Rate limiter waiting for token OR timeout
- Server handling multiple client connections

### What goes wrong when engineers misunderstand select?

1. **Assuming order**: Multiple ready cases are selected randomly
2. **Busy loops**: Forgetting `default` makes select blocking (sometimes wanted)
3. **Priority mistakes**: No priority among cases (except via tricks)
4. **Starvation**: High-frequency channel starves low-frequency ones
5. **Goroutine leaks**: No `ctx.Done()` case means no cancellation

---

## 2. Core Mental Model

### select is a channel switch

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       SELECT MENTAL MODEL                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   select {                                                              │
│   case <-ch1:    │                                                      │
│   case <-ch2:    │──► One case is chosen (randomly if multiple ready)   │
│   case ch3 <- v: │                                                      │
│   default:       │──► Runs immediately if no other case ready           │
│   }                                                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Execution Flow:                                                       │
│                                                                         │
│   1. All case expressions are evaluated (in order)                      │
│   2. All channels are examined                                          │
│   3. If any channel operation can proceed:                              │
│      → One is chosen (pseudo-random among ready cases)                  │
│      → That case executes                                               │
│   4. If no channel operation can proceed:                               │
│      → If default exists: default runs                                  │
│      → If no default: select blocks until one case ready                │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Key insight: select is like a switch for channel operations           │
│                                                                         │
│   switch:  examines values      select:  examines channel states        │
│   ───────────────────────       ─────────────────────────────           │
│   switch x {                    select {                                │
│   case 1:                       case <-ch1:  (ch1 readable?)            │
│   case 2:                       case ch2<-v: (ch2 writable?)            │
│   default:                      default:                                │
│   }                             }                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Blocking vs Non-blocking

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   BLOCKING VS NON-BLOCKING SELECT                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   BLOCKING (no default):                                                │
│   ──────────────────────                                                │
│   select {                                                              │
│   case v := <-ch:     // Waits here until ch has data                   │
│       process(v)                                                        │
│   case <-done:        // Or until done signal                           │
│       return                                                            │
│   }                                                                     │
│   // Control flow continues only after one case completes               │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   NON-BLOCKING (with default):                                          │
│   ────────────────────────────                                          │
│   select {                                                              │
│   case v := <-ch:     // Try to receive                                 │
│       process(v)                                                        │
│   default:            // If ch is empty, run this immediately           │
│       doSomethingElse()                                                 │
│   }                                                                     │
│   // Control flow continues immediately                                 │
│                                                                         │
│   WARNING: Non-blocking select in a loop = busy wait (CPU spin)         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Basic select syntax

```go
select {
case v := <-ch1:
    // Received v from ch1
case ch2 <- value:
    // Sent value to ch2
case v, ok := <-ch3:
    // Received v; ok is false if ch3 is closed
default:
    // No channel ready, execute immediately
}
```

### Selection semantics

```go
// Case expressions are evaluated in order BEFORE select blocks
ch := getChannel()  // Called once
val := getValue()   // Called once

select {
case ch <- val:     // ch and val already evaluated
case <-done:
}
```

### Empty select blocks forever

```go
select {}  // Blocks forever (useful for keeping main alive)
```

### nil channels are never selected

```go
var ch chan int  // nil

select {
case <-ch:       // Never selected (nil channel)
case <-done:     // This will be selected
}
```

---

## 4. Idiomatic Usage

### Pattern 1: Timeout

```go
select {
case result := <-resultCh:
    return result, nil
case <-time.After(5 * time.Second):
    return nil, errors.New("timeout waiting for result")
}
```

**Warning about time.After in loops:**

```go
// BAD: Creates new timer each iteration, memory leak under load
for {
    select {
    case v := <-ch:
        process(v)
    case <-time.After(time.Second):  // New timer every loop!
        log.Println("no data")
    }
}

// GOOD: Reuse timer
timer := time.NewTimer(time.Second)
defer timer.Stop()

for {
    select {
    case v := <-ch:
        if !timer.Stop() {
            <-timer.C
        }
        timer.Reset(time.Second)
        process(v)
    case <-timer.C:
        timer.Reset(time.Second)
        log.Println("no data")
    }
}
```

### Pattern 2: Context cancellation (MANDATORY pattern)

```go
func worker(ctx context.Context, jobs <-chan Job) {
    for {
        select {
        case <-ctx.Done():
            log.Printf("worker cancelled: %v", ctx.Err())
            return
        case job, ok := <-jobs:
            if !ok {
                return  // Channel closed
            }
            process(job)
        }
    }
}
```

### Pattern 3: Non-blocking send

```go
// Try to send, but don't block if buffer full
select {
case ch <- value:
    // Sent successfully
default:
    // Channel full, handle backpressure
    log.Println("channel full, dropping message")
}
```

### Pattern 4: Non-blocking receive

```go
// Check if data available without blocking
select {
case v := <-ch:
    // Data was available
    process(v)
default:
    // No data right now
}
```

### Pattern 5: Priority select (using nested select)

```go
// Always prefer high-priority channel when both ready
for {
    select {
    case <-highPriority:
        handleHigh()
    default:
        select {
        case <-highPriority:
            handleHigh()
        case <-lowPriority:
            handleLow()
        }
    }
}
```

### Pattern 6: Quit channel

```go
func worker(quit <-chan struct{}) {
    for {
        select {
        case <-quit:
            return
        default:
            // Do work
            doWork()
        }
    }
}

// Signal quit by closing the channel (broadcasts to all receivers)
quit := make(chan struct{})
go worker(quit)
go worker(quit)
close(quit)  // Both workers stop
```

---

## 5. Common Pitfalls

### Pitfall 1: Assuming case order matters

```go
// WRONG ASSUMPTION: ch1 has priority because it's first
select {
case <-ch1:  // NOT prioritized!
case <-ch2:  // Equally likely if both ready
}

// If both ch1 and ch2 are ready, Go randomly picks one
// This is by design - prevents starvation
```

### Pitfall 2: Busy loop with default

```go
// BAD: CPU spin - default runs immediately in tight loop
for {
    select {
    case v := <-ch:
        process(v)
    default:
        // This runs constantly when ch is empty!
        // 100% CPU usage
    }
}

// FIX: Remove default for blocking behavior
for {
    select {
    case v := <-ch:
        process(v)
    case <-done:
        return
    }
}

// Or add sleep in default
for {
    select {
    case v := <-ch:
        process(v)
    default:
        time.Sleep(10 * time.Millisecond)  // Rate limit
    }
}
```

### Pitfall 3: Forgetting ctx.Done()

```go
// BAD: No way to cancel this goroutine
func worker(ch <-chan Job) {
    for job := range ch {  // Blocks forever if ch never closes
        process(job)
    }
}

// GOOD: Always include cancellation
func worker(ctx context.Context, ch <-chan Job) {
    for {
        select {
        case <-ctx.Done():
            return
        case job, ok := <-ch:
            if !ok {
                return
            }
            process(job)
        }
    }
}
```

### Pitfall 4: time.After memory leak

```go
// BAD: In high-frequency loop, creates thousands of timers
for msg := range highFrequencyChannel {
    select {
    case <-time.After(time.Second):  // Leak!
        // timeout
    default:
        process(msg)
    }
}

// GOOD: Reuse timer
timer := time.NewTimer(time.Second)
for msg := range highFrequencyChannel {
    timer.Reset(time.Second)
    select {
    case <-timer.C:
        // timeout
    default:
        process(msg)
    }
}
timer.Stop()
```

### Pitfall 5: Sending to possibly-closed channel

```go
// PANIC: If ch might be closed
select {
case ch <- value:  // Panic if ch is closed!
default:
}

// FIX: Use sync or different signaling
// Option 1: Only sender closes, never send after close
// Option 2: Use done channel pattern
```

### Pitfall 6: Nil channel surprise

```go
var ch chan int  // nil!

select {
case <-ch:       // Never executes (nil channel blocks forever)
    fmt.Println("received")
case <-time.After(time.Second):
    fmt.Println("timeout")  // This executes
}

// This is actually USEFUL for dynamic case disabling
func processDynamic(optionalCh <-chan int, required <-chan int) {
    for {
        select {
        case v := <-optionalCh:  // Disabled if nil
            handleOptional(v)
        case v := <-required:
            handleRequired(v)
        }
    }
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "math/rand"
    "sync"
    "time"
)

// RequestRouter demonstrates select for routing with timeouts and priorities
type RequestRouter struct {
    highPriority chan Request
    lowPriority  chan Request
    results      chan Result
    ctx          context.Context
    cancel       context.CancelFunc
    wg           sync.WaitGroup
    
    // Metrics
    processed   int64
    timeouts    int64
    cancelled   int64
}

type Request struct {
    ID       int
    Priority string
    Payload  string
}

type Result struct {
    RequestID int
    Success   bool
    Error     error
    Latency   time.Duration
}

func NewRequestRouter(highBuffer, lowBuffer int) *RequestRouter {
    ctx, cancel := context.WithCancel(context.Background())
    return &RequestRouter{
        highPriority: make(chan Request, highBuffer),
        lowPriority:  make(chan Request, lowBuffer),
        results:      make(chan Result, highBuffer+lowBuffer),
        ctx:          ctx,
        cancel:       cancel,
    }
}

func (r *RequestRouter) Start(workers int) {
    log.Printf("Starting router with %d workers", workers)
    
    for i := 0; i < workers; i++ {
        r.wg.Add(1)
        go r.worker(i)
    }
    
    // Result collector
    r.wg.Add(1)
    go r.collector()
}

func (r *RequestRouter) worker(id int) {
    defer r.wg.Done()
    
    // Per-worker timeout timer (reused, not leaked)
    requestTimeout := 100 * time.Millisecond
    timer := time.NewTimer(requestTimeout)
    defer timer.Stop()
    
    for {
        // Priority select: Always check high priority first
        select {
        case <-r.ctx.Done():
            log.Printf("Worker %d: shutting down", id)
            return
        case req := <-r.highPriority:
            r.handleRequest(id, req, timer, requestTimeout)
        default:
            // High priority empty, check both queues
            select {
            case <-r.ctx.Done():
                log.Printf("Worker %d: shutting down", id)
                return
            case req := <-r.highPriority:
                r.handleRequest(id, req, timer, requestTimeout)
            case req := <-r.lowPriority:
                r.handleRequest(id, req, timer, requestTimeout)
            }
        }
    }
}

func (r *RequestRouter) handleRequest(workerID int, req Request, timer *time.Timer, timeout time.Duration) {
    start := time.Now()
    
    // Reset timer for this request
    if !timer.Stop() {
        select {
        case <-timer.C:
        default:
        }
    }
    timer.Reset(timeout)
    
    // Simulate processing with potential timeout
    resultCh := make(chan error, 1)
    go func() {
        resultCh <- r.processRequest(req)
    }()
    
    var result Result
    result.RequestID = req.ID
    
    select {
    case <-r.ctx.Done():
        result.Error = r.ctx.Err()
        r.cancelled++
    case <-timer.C:
        result.Error = errors.New("request timeout")
        r.timeouts++
    case err := <-resultCh:
        result.Error = err
        result.Success = (err == nil)
        r.processed++
    }
    
    result.Latency = time.Since(start)
    
    // Non-blocking send to results (with backpressure handling)
    select {
    case r.results <- result:
    case <-r.ctx.Done():
    default:
        log.Printf("Worker %d: results channel full, dropping result", workerID)
    }
}

func (r *RequestRouter) processRequest(req Request) error {
    // Simulate variable processing time
    processingTime := time.Duration(rand.Intn(150)) * time.Millisecond
    time.Sleep(processingTime)
    
    // Simulate occasional failures
    if rand.Float32() < 0.1 {
        return errors.New("simulated processing error")
    }
    
    return nil
}

func (r *RequestRouter) collector() {
    defer r.wg.Done()
    
    var successCount, failCount int
    
    for {
        select {
        case <-r.ctx.Done():
            log.Printf("Collector: done (success=%d, fail=%d)", successCount, failCount)
            return
        case result, ok := <-r.results:
            if !ok {
                return
            }
            if result.Success {
                successCount++
            } else {
                failCount++
                if result.Error != nil {
                    log.Printf("Request %d failed: %v (latency: %v)",
                        result.RequestID, result.Error, result.Latency)
                }
            }
        }
    }
}

// SubmitHigh submits a high-priority request
func (r *RequestRouter) SubmitHigh(ctx context.Context, req Request) error {
    req.Priority = "high"
    select {
    case <-ctx.Done():
        return ctx.Err()
    case <-r.ctx.Done():
        return errors.New("router shutdown")
    case r.highPriority <- req:
        return nil
    case <-time.After(time.Second):
        return errors.New("submit timeout: high priority queue full")
    }
}

// SubmitLow submits a low-priority request
func (r *RequestRouter) SubmitLow(ctx context.Context, req Request) error {
    req.Priority = "low"
    select {
    case <-ctx.Done():
        return ctx.Err()
    case <-r.ctx.Done():
        return errors.New("router shutdown")
    case r.lowPriority <- req:
        return nil
    default:
        return errors.New("low priority queue full")
    }
}

func (r *RequestRouter) Shutdown(timeout time.Duration) {
    log.Println("Initiating router shutdown...")
    
    // Close input channels
    close(r.highPriority)
    close(r.lowPriority)
    
    // Cancel context
    r.cancel()
    
    // Wait with timeout
    done := make(chan struct{})
    go func() {
        r.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("Router shutdown complete")
    case <-time.After(timeout):
        log.Println("Router shutdown timeout")
    }
    
    close(r.results)
    
    log.Printf("Final stats: processed=%d, timeouts=%d, cancelled=%d",
        r.processed, r.timeouts, r.cancelled)
}

// Demonstrate various select patterns
func demonstrateSelectPatterns() {
    fmt.Println("\n=== Select Patterns Demo ===")
    
    // Pattern 1: Timeout
    fmt.Println("\n--- Timeout Pattern ---")
    ch := make(chan int)
    select {
    case v := <-ch:
        fmt.Printf("Received: %d\n", v)
    case <-time.After(100 * time.Millisecond):
        fmt.Println("Timeout after 100ms")
    }
    
    // Pattern 2: Non-blocking check
    fmt.Println("\n--- Non-blocking Check ---")
    buffered := make(chan int, 1)
    buffered <- 42
    
    select {
    case v := <-buffered:
        fmt.Printf("Got value: %d\n", v)
    default:
        fmt.Println("No value available")
    }
    
    select {
    case v := <-buffered:
        fmt.Printf("Got value: %d\n", v)
    default:
        fmt.Println("No value available (channel empty now)")
    }
    
    // Pattern 3: Multi-channel receive
    fmt.Println("\n--- Multi-channel Receive ---")
    ch1 := make(chan string, 1)
    ch2 := make(chan string, 1)
    ch1 <- "from ch1"
    ch2 <- "from ch2"
    
    for i := 0; i < 2; i++ {
        select {
        case msg := <-ch1:
            fmt.Printf("Received: %s\n", msg)
        case msg := <-ch2:
            fmt.Printf("Received: %s\n", msg)
        }
    }
    
    // Pattern 4: Dynamic channel enable/disable with nil
    fmt.Println("\n--- Dynamic Channel Disable ---")
    var optionalCh chan int  // nil - disabled
    requiredCh := make(chan int, 1)
    requiredCh <- 100
    
    select {
    case v := <-optionalCh:  // Never selected (nil)
        fmt.Printf("Optional: %d\n", v)
    case v := <-requiredCh:
        fmt.Printf("Required: %d\n", v)
    }
}

func main() {
    rand.Seed(time.Now().UnixNano())
    
    demonstrateSelectPatterns()
    
    fmt.Println("\n=== Request Router Demo ===")
    
    router := NewRequestRouter(10, 20)
    router.Start(3)
    
    ctx := context.Background()
    
    // Submit mixed requests
    for i := 0; i < 20; i++ {
        req := Request{ID: i, Payload: fmt.Sprintf("request-%d", i)}
        
        var err error
        if i%3 == 0 {
            err = router.SubmitHigh(ctx, req)
        } else {
            err = router.SubmitLow(ctx, req)
        }
        
        if err != nil {
            log.Printf("Failed to submit request %d: %v", i, err)
        }
    }
    
    // Let requests process
    time.Sleep(time.Second)
    
    router.Shutdown(5 * time.Second)
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 7. Design Takeaways

### Rules of Thumb

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       SELECT DESIGN RULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ALWAYS include ctx.Done() case                                     │
│      • Every select in production code should be cancellable            │
│      • Without it, goroutines can leak                                  │
│                                                                         │
│   2. AVOID default unless you need non-blocking                         │
│      • default makes select non-blocking                                │
│      • In a loop, this becomes a busy-wait                              │
│                                                                         │
│   3. DON'T assume case priority                                         │
│      • Ready cases are selected randomly                                │
│      • Use nested select for priority if needed                         │
│                                                                         │
│   4. REUSE timers in loops                                              │
│      • time.After creates new timer each call                           │
│      • Use time.NewTimer + Reset in hot paths                           │
│                                                                         │
│   5. USE nil channels to disable cases                                  │
│      • Nil channel blocks forever                                       │
│      • Set channel to nil to dynamically disable                        │
│                                                                         │
│   6. HANDLE closed channels                                             │
│      • Closed channel returns zero value immediately                    │
│      • Use v, ok := <-ch pattern when needed                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### The Universal Worker Pattern

```go
// This is the canonical Go worker pattern
func worker(ctx context.Context, jobs <-chan Job, results chan<- Result) {
    for {
        select {
        case <-ctx.Done():
            return  // Clean shutdown
        case job, ok := <-jobs:
            if !ok {
                return  // Channel closed
            }
            result := process(job)
            select {
            case <-ctx.Done():
                return
            case results <- result:
            }
        }
    }
}
```

---

## Chinese Explanation (中文解释)

### select 核心概念

**select 是用于 channel 操作的多路复用语句，类似于 switch 但用于 channel。**

### 执行流程

1. 所有 case 表达式被求值（按顺序）
2. 检查所有 channel 的状态
3. 如果有多个 case 可以执行，**随机**选择一个
4. 如果没有 case 可以执行：
   - 有 `default`：立即执行 default
   - 无 `default`：阻塞等待

### 阻塞 vs 非阻塞

| 类型 | 特征 | 用途 |
|------|------|------|
| 阻塞（无 default） | 等待直到某个 case 就绪 | 正常的等待场景 |
| 非阻塞（有 default） | 立即返回，不等待 | 检查是否有数据 |

### 关键模式

1. **超时模式**
```go
select {
case result := <-ch:
    return result
case <-time.After(5 * time.Second):
    return nil, errors.New("timeout")
}
```

2. **取消模式**（必须掌握）
```go
select {
case <-ctx.Done():
    return ctx.Err()
case job := <-jobs:
    process(job)
}
```

3. **非阻塞发送**
```go
select {
case ch <- value:
    // 发送成功
default:
    // channel 满了，处理背压
}
```

### 常见错误

1. **假设 case 有优先级**：实际上是随机选择的
2. **循环中使用 default 导致 CPU 空转**
3. **忘记 ctx.Done() 导致 goroutine 泄漏**
4. **循环中使用 time.After 导致内存泄漏**
5. **向可能关闭的 channel 发送导致 panic**

### 设计原则

1. **生产代码的 select 必须包含 ctx.Done()**
2. **除非需要非阻塞，否则不要用 default**
3. **在热路径中重用 timer**
4. **使用 nil channel 动态禁用 case**

