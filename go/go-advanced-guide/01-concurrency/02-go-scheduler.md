# Go Scheduler: The M:N Runtime Engine

## 1. Engineering Problem

### What real-world problem does this solve?

**The fundamental tension: OS threads are expensive, but we need parallelism.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     THREADING MODEL COMPARISON                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1:1 Model (C/C++, Java, Rust)                                         │
│   ─────────────────────────────                                         │
│   User Thread ────────► OS Thread                                       │
│                                                                         │
│   Cost: ~1-8MB per thread, ~1-10µs context switch                       │
│   Limit: ~10K threads practical maximum                                 │
│   Problem: 100K connections = 100K threads = crash                      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   N:1 Model (Green Threads, early Python/Ruby)                          │
│   ────────────────────────────────────────────                          │
│   N User Threads ────────► 1 OS Thread                                  │
│                                                                         │
│   Problem: No parallelism, one blocking call blocks everything          │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   M:N Model (Go, Erlang, some JVM fibers)                               │
│   ───────────────────────────────────────                               │
│   N Goroutines ────────► M OS Threads (M << N)                          │
│                                                                         │
│   Best of both: Parallelism + Lightweight concurrency                   │
│   100K goroutines on 8 OS threads = works                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when engineers misunderstand the scheduler?

1. **Assuming execution order**: Goroutines don't run in the order they're created
2. **Blocking the scheduler**: CGGO calls, CPU-bound loops without yields
3. **Misunderstanding GOMAXPROCS**: It's not "number of goroutines"
4. **Over-tuning**: Trying to control what should be left to the runtime
5. **Not understanding preemption**: Long-running goroutines can starve others

---

## 2. Core Mental Model

### The GMP Model

Go's scheduler uses three core entities:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           GMP MODEL                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   G (Goroutine)           M (Machine)           P (Processor)           │
│   ─────────────           ───────────           ─────────────           │
│   • Your code             • OS thread           • Scheduling context    │
│   • 2KB initial stack     • Created by OS       • Count = GOMAXPROCS    │
│   • Millions possible     • Limited (~10K)      • Has local run queue   │
│   • Managed by runtime    • Heavy resource      • Required to run G     │
│                                                                         │
│                                                                         │
│   ┌───────────────────────────────────────────────────────────────┐     │
│   │                      SCHEDULER STATE                          │     │
│   │                                                               │     │
│   │   Global Run Queue: [ G7, G8, G9, ... ]                       │     │
│   │                           ▲                                   │     │
│   │                           │ (steal when local empty)          │     │
│   │                           │                                   │     │
│   │   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐      │     │
│   │   │   P0    │   │   P1    │   │   P2    │   │   P3    │      │     │
│   │   │─────────│   │─────────│   │─────────│   │─────────│      │     │
│   │   │ Running:│   │ Running:│   │ Running:│   │ Idle    │      │     │
│   │   │   G1    │   │   G3    │   │   G5    │   │   --    │      │     │
│   │   │─────────│   │─────────│   │─────────│   │         │      │     │
│   │   │ Local Q:│   │ Local Q:│   │ Local Q:│   │         │      │     │
│   │   │ [G2]    │   │ [G4]    │   │ [G6]    │   │         │      │     │
│   │   └────┬────┘   └────┬────┘   └────┬────┘   └─────────┘      │     │
│   │        │             │             │                          │     │
│   │        ▼             ▼             ▼                          │     │
│   │   ┌─────────┐   ┌─────────┐   ┌─────────┐                    │     │
│   │   │   M0    │   │   M1    │   │   M2    │                    │     │
│   │   │(thread) │   │(thread) │   │(thread) │   M3: parked       │     │
│   │   └─────────┘   └─────────┘   └─────────┘                    │     │
│   │                                                               │     │
│   └───────────────────────────────────────────────────────────────┘     │
│                                                                         │
│   Key Insight: P is the bottleneck, not M                               │
│   • Only GOMAXPROCS goroutines can truly run in parallel               │
│   • M can exceed GOMAXPROCS (blocked in syscalls)                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Scheduling Philosophy

1. **Work-stealing**: Idle P steals work from busy P's local queue
2. **Hand-off**: When M blocks in syscall, P is handed off to another M
3. **Cooperative + Preemptive**: Goroutines yield at safe points; since Go 1.14, async preemption for long-running goroutines
4. **Local-first**: Reduces contention by preferring local queue operations

---

## 3. Language Mechanism

### GOMAXPROCS

```go
import "runtime"

// Get current value
n := runtime.GOMAXPROCS(0)

// Set value (returns old value)
old := runtime.GOMAXPROCS(8)

// Default: number of CPU cores
// runtime.GOMAXPROCS(runtime.NumCPU()) is the default
```

**What GOMAXPROCS actually means:**
- Number of P (processors) = Number of goroutines that can execute **simultaneously**
- NOT the maximum number of goroutines (that's unlimited)
- NOT the number of OS threads (M can exceed this during syscalls)

### Scheduling Points (Where Goroutines Yield)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    WHEN GOROUTINES YIELD                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Cooperative Yields:                                                   │
│   ──────────────────                                                    │
│   • Channel send/receive                                                │
│   • select statement                                                    │
│   • sync.Mutex/RWMutex lock                                             │
│   • sync.WaitGroup.Wait()                                               │
│   • time.Sleep()                                                        │
│   • I/O operations (network, file)                                      │
│   • runtime.Gosched() (explicit yield)                                  │
│   • Function calls (stack check may trigger yield)                      │
│                                                                         │
│   Async Preemption (Go 1.14+):                                          │
│   ────────────────────────────                                          │
│   • Long-running goroutines are preempted via signals                   │
│   • Prevents CPU-bound loops from starving others                       │
│   • Uses SIGURG on Unix systems                                         │
│                                                                         │
│   Before Go 1.14:                                                       │
│   ────────────────                                                      │
│   for { /* tight loop with no function calls */ }  // STARVES others    │
│                                                                         │
│   After Go 1.14:                                                        │
│   ────────────────                                                      │
│   // Same loop is now preemptible                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### System Call Handling

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYSCALL HANDLING                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Before Syscall:        During Syscall:        After Syscall:          │
│   ────────────────       ───────────────        ──────────────          │
│                                                                         │
│   ┌─────┐                ┌─────┐                ┌─────┐                 │
│   │ G1  │ executing      │ G1  │ blocked        │ G1  │ runnable        │
│   └──┬──┘                └──┬──┘                └──┬──┘                 │
│      │                      │                      │                    │
│   ┌──▼──┐                ┌──▼──┐                   │                    │
│   │ P0  │                │     │ (detached)     ┌──▼──┐                 │
│   └──┬──┘                └─────┘                │ P?  │ (any P)         │
│      │                      │                   └──┬──┘                 │
│   ┌──▼──┐                ┌──▼──┐                ┌──▼──┐                 │
│   │ M0  │                │ M0  │ in kernel      │ M?  │ (any M)         │
│   └─────┘                └─────┘                └─────┘                 │
│                                                                         │
│   P is handed off to another M so other Gs can run!                     │
│   When syscall completes, G1 goes back to a run queue.                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Runtime Inspection

```go
import "runtime"

// Number of goroutines
runtime.NumGoroutine()

// Number of CPUs
runtime.NumCPU()

// Force garbage collection (rarely needed)
runtime.GC()

// Yield to scheduler (rarely needed)
runtime.Gosched()

// Lock current goroutine to current OS thread
runtime.LockOSThread()
defer runtime.UnlockOSThread()
```

---

## 4. Idiomatic Usage

### When to think about the scheduler

✅ **Relevant situations:**
- Tuning GOMAXPROCS for containers (cgroups may not reflect reality)
- Debugging performance issues with CPU profiling
- CGO-heavy workloads (C calls block M)
- Real-time requirements (GC pauses, scheduling latency)

❌ **Don't over-tune:**
- The default GOMAXPROCS is usually correct
- Don't manually call `runtime.Gosched()` unless profiling shows starvation
- Don't try to control which goroutine runs when

### Container-aware GOMAXPROCS

```go
import (
    "runtime"
    
    "go.uber.org/automaxprocs/maxprocs"  // Uber's library
)

func main() {
    // Automatically set GOMAXPROCS based on cgroup limits
    // Critical for Kubernetes/Docker where CPU limits != NumCPU
    undo, err := maxprocs.Set()
    defer undo()
    if err != nil {
        log.Printf("failed to set GOMAXPROCS: %v", err)
    }
    
    // ...
}
```

### When to use LockOSThread

```go
// Use case 1: Thread-local storage in C libraries (OpenGL, etc.)
runtime.LockOSThread()
defer runtime.UnlockOSThread()
initOpenGLContext()  // Must stay on same thread

// Use case 2: Setting process attributes that are per-thread
runtime.LockOSThread()
syscall.Setns(fd, syscall.CLONE_NEWNET)  // Linux namespace
// Goroutine now in different network namespace
```

---

## 5. Common Pitfalls

### Pitfall 1: CPU-bound loops before Go 1.14

```go
// Before Go 1.14: This could starve other goroutines
func cpuIntensive() {
    for i := 0; i < 1000000000; i++ {
        // No function calls, no yields
        result += i * i
    }
}

// Fix for pre-1.14 (still good practice):
func cpuIntensive() {
    for i := 0; i < 1000000000; i++ {
        if i % 1000000 == 0 {
            runtime.Gosched()  // Explicit yield
        }
        result += i * i
    }
}
```

### Pitfall 2: CGO blocks M

```go
/*
#include <unistd.h>
void slowCFunction() {
    sleep(10);  // Blocks the OS thread!
}
*/
import "C"

func callC() {
    // This blocks M for 10 seconds
    // P will be handed off, but M is unavailable
    // If all Ms are blocked in CGO, scheduler stalls
    C.slowCFunction()
}

// Mitigation: Use worker pool for CGO calls
var cgoSemaphore = make(chan struct{}, runtime.GOMAXPROCS(0))

func callCSafe() {
    cgoSemaphore <- struct{}{}  // Limit concurrent CGO calls
    defer func() { <-cgoSemaphore }()
    C.slowCFunction()
}
```

### Pitfall 3: Wrong GOMAXPROCS in containers

```go
// BUG: Container has 2 CPU cores, but host has 64
// runtime.NumCPU() returns 64!
// Result: 64 Ps competing for 2 cores = thrashing

// FIX: Use automaxprocs or set manually
import _ "go.uber.org/automaxprocs"

// Or explicitly:
func init() {
    // Read from cgroup
    quota, period := readCGroupCPU()
    if quota > 0 && period > 0 {
        runtime.GOMAXPROCS(int(quota / period))
    }
}
```

### Pitfall 4: Assuming goroutine execution order

```go
// BUG: Assuming g1 runs before g2
go g1()  // Might run second
go g2()  // Might run first

// No guarantee! Both are placed in run queues
// Scheduler decides order based on many factors
```

### Pitfall 5: Misusing LockOSThread

```go
// WRONG: Locking thread without cleanup
func handler(w http.ResponseWriter, r *http.Request) {
    runtime.LockOSThread()
    // Forgot to unlock!
    // This M is now unusable for other goroutines
    // Memory leak of OS threads
    
    // CORRECT:
    runtime.LockOSThread()
    defer runtime.UnlockOSThread()
}
```

---

## 6. Complete, Realistic Example

This example demonstrates scheduler-aware programming for a CPU-intensive service:

```go
package main

import (
    "context"
    "fmt"
    "log"
    "runtime"
    "sync"
    "sync/atomic"
    "time"
)

// SchedulerStats tracks scheduler-related metrics
type SchedulerStats struct {
    goroutines  atomic.Int64
    completed   atomic.Int64
    cpuBoundOps atomic.Int64
}

func (s *SchedulerStats) String() string {
    return fmt.Sprintf(
        "goroutines=%d completed=%d cpu_ops=%d",
        s.goroutines.Load(),
        s.completed.Load(),
        s.cpuBoundOps.Load(),
    )
}

// WorkerPool demonstrates scheduler-friendly concurrent processing
type WorkerPool struct {
    workers     int
    jobQueue    chan Job
    results     chan Result
    wg          sync.WaitGroup
    ctx         context.Context
    cancel      context.CancelFunc
    stats       *SchedulerStats
    cpuSemaphore chan struct{}  // Limit CPU-bound operations
}

type Job struct {
    ID        int
    CPUBound  bool
    WorkUnits int
}

type Result struct {
    JobID   int
    Value   int64
    Elapsed time.Duration
}

// NewWorkerPool creates a pool that respects scheduler constraints
func NewWorkerPool(workers int) *WorkerPool {
    ctx, cancel := context.WithCancel(context.Background())
    
    // Limit concurrent CPU-bound operations to GOMAXPROCS
    // This prevents overwhelming the scheduler
    cpuLimit := runtime.GOMAXPROCS(0)
    
    return &WorkerPool{
        workers:      workers,
        jobQueue:     make(chan Job, workers*2),
        results:      make(chan Result, workers*2),
        ctx:          ctx,
        cancel:       cancel,
        stats:        &SchedulerStats{},
        cpuSemaphore: make(chan struct{}, cpuLimit),
    }
}

func (p *WorkerPool) Start() {
    log.Printf("Starting pool: workers=%d GOMAXPROCS=%d NumCPU=%d",
        p.workers, runtime.GOMAXPROCS(0), runtime.NumCPU())
    
    for i := 0; i < p.workers; i++ {
        p.wg.Add(1)
        p.stats.goroutines.Add(1)
        go p.worker(i)
    }
    
    // Background goroutine to monitor scheduler
    go p.monitor()
}

func (p *WorkerPool) worker(id int) {
    defer p.wg.Done()
    defer p.stats.goroutines.Add(-1)
    
    for {
        select {
        case <-p.ctx.Done():
            log.Printf("Worker %d: shutting down", id)
            return
            
        case job, ok := <-p.jobQueue:
            if !ok {
                return
            }
            
            start := time.Now()
            result := p.processJob(job)
            result.Elapsed = time.Since(start)
            
            select {
            case p.results <- result:
            case <-p.ctx.Done():
                return
            }
            
            p.stats.completed.Add(1)
        }
    }
}

func (p *WorkerPool) processJob(job Job) Result {
    if job.CPUBound {
        // Acquire semaphore for CPU-bound work
        // This prevents all workers from doing CPU work simultaneously
        select {
        case p.cpuSemaphore <- struct{}{}:
            defer func() { <-p.cpuSemaphore }()
        case <-p.ctx.Done():
            return Result{JobID: job.ID}
        }
        
        p.stats.cpuBoundOps.Add(1)
        value := p.cpuIntensiveWork(job.WorkUnits)
        return Result{JobID: job.ID, Value: value}
    }
    
    // I/O-bound simulation
    time.Sleep(time.Duration(job.WorkUnits) * time.Millisecond)
    return Result{JobID: job.ID, Value: int64(job.WorkUnits)}
}

func (p *WorkerPool) cpuIntensiveWork(units int) int64 {
    // CPU-intensive computation
    // Note: This will be preempted by Go 1.14+ scheduler
    var result int64
    for i := 0; i < units*1000000; i++ {
        result += int64(i * i % 1000)
    }
    return result
}

func (p *WorkerPool) monitor() {
    ticker := time.NewTicker(time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-p.ctx.Done():
            return
        case <-ticker.C:
            log.Printf("Monitor: total_goroutines=%d pool_stats=%s",
                runtime.NumGoroutine(), p.stats)
        }
    }
}

func (p *WorkerPool) Submit(job Job) error {
    select {
    case p.jobQueue <- job:
        return nil
    case <-p.ctx.Done():
        return p.ctx.Err()
    case <-time.After(time.Second):
        return fmt.Errorf("job queue full")
    }
}

func (p *WorkerPool) Results() <-chan Result {
    return p.results
}

func (p *WorkerPool) Shutdown(timeout time.Duration) {
    log.Println("Shutting down worker pool...")
    
    // Stop accepting new jobs
    close(p.jobQueue)
    
    // Wait for workers with timeout
    done := make(chan struct{})
    go func() {
        p.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("All workers finished")
    case <-time.After(timeout):
        log.Println("Shutdown timeout, cancelling...")
        p.cancel()
    }
    
    close(p.results)
}

// demonstrateSchedulerBehavior shows different scheduling scenarios
func demonstrateSchedulerBehavior() {
    fmt.Println("\n=== Scheduler Behavior Demo ===")
    
    // Show current scheduler configuration
    fmt.Printf("GOMAXPROCS: %d\n", runtime.GOMAXPROCS(0))
    fmt.Printf("NumCPU: %d\n", runtime.NumCPU())
    fmt.Printf("Initial goroutines: %d\n", runtime.NumGoroutine())
    
    // Create workers based on GOMAXPROCS
    pool := NewWorkerPool(runtime.GOMAXPROCS(0) * 2)
    pool.Start()
    
    // Collect results in background
    var resultWg sync.WaitGroup
    resultWg.Add(1)
    go func() {
        defer resultWg.Done()
        count := 0
        for result := range pool.Results() {
            if count < 5 {  // Only log first few
                log.Printf("Result: job=%d value=%d elapsed=%v",
                    result.JobID, result.Value, result.Elapsed)
            }
            count++
        }
        log.Printf("Total results collected: %d", count)
    }()
    
    // Submit mixed workload
    for i := 0; i < 20; i++ {
        job := Job{
            ID:        i,
            CPUBound:  i%2 == 0,  // Alternate CPU and I/O bound
            WorkUnits: 10,
        }
        if err := pool.Submit(job); err != nil {
            log.Printf("Failed to submit job %d: %v", i, err)
        }
    }
    
    // Let work complete
    time.Sleep(3 * time.Second)
    
    pool.Shutdown(5 * time.Second)
    resultWg.Wait()
    
    fmt.Printf("Final goroutines: %d\n", runtime.NumGoroutine())
}

// demonstrateWorkStealing shows work stealing behavior
func demonstrateWorkStealing() {
    fmt.Println("\n=== Work Stealing Demo ===")
    
    var wg sync.WaitGroup
    start := time.Now()
    
    // Create uneven workload distribution
    // Some goroutines do more work than others
    // Scheduler will steal work to balance
    
    for p := 0; p < runtime.GOMAXPROCS(0); p++ {
        workload := 100 + p*50  // Uneven work per "P"
        
        for j := 0; j < workload; j++ {
            wg.Add(1)
            go func(id int) {
                defer wg.Done()
                // Small amount of work
                sum := 0
                for i := 0; i < 10000; i++ {
                    sum += i
                }
                _ = sum
            }(j)
        }
    }
    
    wg.Wait()
    elapsed := time.Since(start)
    
    fmt.Printf("Completed uneven workload in %v\n", elapsed)
    fmt.Println("(Work stealing balances load across Ps)")
}

func main() {
    // Print scheduler configuration
    fmt.Println("=== Go Scheduler Configuration ===")
    fmt.Printf("Go version: %s\n", runtime.Version())
    fmt.Printf("GOMAXPROCS: %d\n", runtime.GOMAXPROCS(0))
    fmt.Printf("NumCPU: %d\n", runtime.NumCPU())
    
    demonstrateSchedulerBehavior()
    demonstrateWorkStealing()
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 7. Design Takeaways

### Rules of Thumb

1. **Trust the defaults**
   - `GOMAXPROCS = NumCPU` is usually right
   - Only tune when profiling shows problems
   - In containers, use `automaxprocs`

2. **CPU-bound work needs limits**
   - Use semaphores to cap concurrent CPU ops
   - Consider `GOMAXPROCS` as your parallelism budget

3. **CGO is expensive**
   - Each CGO call can block an M
   - Use worker pools for CGO-heavy code
   - Consider `LockOSThread` implications

4. **The scheduler is your friend**
   - Don't try to control scheduling
   - Design for cooperation, not competition
   - Yield naturally through channels and sync primitives

5. **Monitor in production**
   - Track `runtime.NumGoroutine()`
   - Watch for runaway goroutine counts
   - Profile CPU and scheduler latency

### Architecture Implications

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 SCHEDULER-AWARE ARCHITECTURE                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                     Service Design                               │   │
│   │                                                                 │   │
│   │   1. Separate I/O-bound and CPU-bound paths                     │   │
│   │   2. Bound CPU-bound concurrency to GOMAXPROCS                  │   │
│   │   3. Let I/O-bound goroutines run freely (they yield often)     │   │
│   │                                                                 │   │
│   │   ┌─────────────┐     ┌─────────────────────────────────────┐   │   │
│   │   │  HTTP/gRPC  │     │        Processing Layers            │   │   │
│   │   │  Handlers   │     │                                     │   │   │
│   │   │  (I/O-bound)│────►│  I/O layer ──► CPU layer ──► I/O   │   │   │
│   │   │  Many OK    │     │  (unbounded)   (bounded)    (many)  │   │   │
│   │   └─────────────┘     └─────────────────────────────────────┘   │   │
│   │                                                                 │   │
│   │   4. Use buffered channels as backpressure mechanism            │   │
│   │   5. Monitor goroutine count as a health metric                 │   │
│   │                                                                 │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Go 调度器核心概念

**Go 使用 M:N 调度模型：N 个 Goroutine 运行在 M 个 OS 线程上。**

1. **G (Goroutine)**：用户代码的执行单元，轻量级，可以创建百万个
2. **M (Machine)**：OS 线程，由操作系统管理，创建成本高
3. **P (Processor)**：调度上下文，数量 = GOMAXPROCS，是真正的并行度

### 调度机制

1. **工作窃取**：空闲的 P 会从其他 P 的本地队列窃取 Goroutine
2. **系统调用处理**：当 M 阻塞在系统调用时，P 会被移交给其他 M
3. **协作式 + 抢占式**：Goroutine 在安全点让出执行权；Go 1.14 后支持异步抢占

### GOMAXPROCS 的真正含义

- 表示可以**同时**执行的 Goroutine 数量
- **不是** Goroutine 的最大数量（那是无限的）
- **不是** OS 线程数量（M 可以超过这个值）
- 默认值 = CPU 核心数

### 容器环境注意事项

在 Kubernetes/Docker 中，`runtime.NumCPU()` 返回的是宿主机的 CPU 数量，而不是容器的 CPU 配额。需要使用 `automaxprocs` 库或手动从 cgroup 读取限制。

### 设计原则

1. **信任默认值**：除非性能分析显示问题，否则不要调整
2. **限制 CPU 密集型操作**：使用信号量控制并发度
3. **CGO 调用代价高**：每次 CGO 调用都会阻塞一个 M
4. **不要试图控制调度**：通过 channel 和 sync 原语自然让出执行权
5. **生产环境监控**：跟踪 `runtime.NumGoroutine()`

