# Goroutines: The Foundation of Go's Execution Model

## 1. Engineering Problem

### What real-world problem does this solve?

Traditional threading models force a brutal trade-off:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE CONCURRENCY DILEMMA                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   OS Threads (C/C++/Java)              Event Loops (Node.js/Python)     │
│   ─────────────────────────            ─────────────────────────────    │
│   ✓ True parallelism                   ✓ Lightweight (millions OK)     │
│   ✓ Blocking I/O is fine               ✓ Simple mental model           │
│   ✗ Heavy: 1-8MB stack per thread      ✗ Single-threaded execution     │
│   ✗ Context switch: ~1-10µs            ✗ Callback hell / async await   │
│   ✗ Practical limit: ~10K threads      ✗ CPU-bound work blocks all     │
│   ✗ C10K problem                       ✗ No true parallelism           │
│                                                                         │
│                              ↓                                          │
│                                                                         │
│   Goroutines: Best of Both Worlds                                       │
│   ───────────────────────────────                                       │
│   ✓ True parallelism (multiple OS threads)                              │
│   ✓ Lightweight: 2KB initial stack (grows as needed)                    │
│   ✓ Millions of goroutines are practical                                │
│   ✓ Blocking calls don't block other goroutines                         │
│   ✓ Sequential code style (no callbacks)                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Real systems this enables:**
- A gRPC server handling 100K concurrent connections (like your `routermgr`)
- A crawler with 10K concurrent HTTP requests
- A message broker routing millions of messages/second

### What goes wrong when engineers misunderstand goroutines?

1. **Goroutine leaks**: Starting goroutines without lifetime management → memory grows indefinitely
2. **Race conditions**: Assuming goroutines run in a predictable order
3. **Resource exhaustion**: Unbounded goroutine creation under load
4. **Deadlocks**: Blocking operations with no cancellation path
5. **Silent failures**: Panics in goroutines that nobody observes

---

## 2. Core Mental Model

### How Go expects you to think

**A goroutine is NOT a thread. It's a unit of concurrent work managed by Go's runtime.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         GO SCHEDULER (M:N MODEL)                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   G = Goroutine (your code)                                             │
│   M = Machine (OS thread)                                               │
│   P = Processor (scheduling context, typically GOMAXPROCS)              │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                        Go Runtime                               │   │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐             │   │
│   │  │   P0    │  │   P1    │  │   P2    │  │   P3    │  (4 CPUs)   │   │
│   │  │ ┌─────┐ │  │ ┌─────┐ │  │ ┌─────┐ │  │ ┌─────┐ │             │   │
│   │  │ │ G1  │ │  │ │ G5  │ │  │ │ G9  │ │  │ │ G13 │ │  Running    │   │
│   │  │ └─────┘ │  │ └─────┘ │  │ └─────┘ │  │ └─────┘ │             │   │
│   │  │ [G2,G3]│  │ [G6,G7]│  │ [G10,11]│  │ [G14,15]│  Local Q      │   │
│   │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘             │   │
│   │       │            │            │            │                  │   │
│   │       ▼            ▼            ▼            ▼                  │   │
│   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐             │   │
│   │  │   M0    │  │   M1    │  │   M2    │  │   M3    │  OS Threads │   │
│   │  └─────────┘  └─────────┘  └─────────┘  └─────────┘             │   │
│   │                                                                 │   │
│   │  Global Run Queue: [G4, G8, G12, G16, G17, G18, ...]            │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   Kernel                                                                │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  CPU0        CPU1        CPU2        CPU3                       │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key insights:

1. **Goroutines are multiplexed onto OS threads** (M:N scheduling)
2. **The scheduler is cooperative, not preemptive** (mostly) — goroutines yield at specific points
3. **Blocking a goroutine does NOT block the OS thread** — the runtime parks the goroutine and runs another
4. **You don't control which thread runs your goroutine** — this is intentional

### Philosophy: "Share memory by communicating"

Go inverts the traditional model:
- **Traditional**: Share memory, use locks to coordinate
- **Go idiom**: Communicate via channels, let data flow between goroutines

---

## 3. Language Mechanism

### Starting a goroutine

```go
go functionCall()           // Starts immediately, runs concurrently
go func() { /* ... */ }()   // Anonymous function, common pattern
```

### What happens when you write `go f()`?

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GOROUTINE CREATION SEQUENCE                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. Runtime allocates a goroutine struct (~2KB initial stack)          │
│   2. Arguments to f() are evaluated NOW (in the calling goroutine)      │
│   3. Goroutine is placed in the local run queue of current P            │
│   4. Control returns to caller IMMEDIATELY                              │
│   5. Scheduler decides when to actually run the new goroutine           │
│                                                                         │
│   CRITICAL: There is NO guarantee when the goroutine will start!        │
│                                                                         │
│   main() ──────────┬────────────────────────────────────────────►       │
│                    │ go f(x)                                            │
│                    │                                                    │
│                    └──────────────────────────────────────────►         │
│                         f(x) starts "sometime later"                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Goroutine states

```
           ┌──────────┐
           │ Runnable │◄────────────────────────────────┐
           └────┬─────┘                                 │
                │ Scheduled                             │
                ▼                                       │
           ┌──────────┐                                 │
           │ Running  │─────────────────────────────────┤
           └────┬─────┘     Preempted / Yield           │
                │                                       │
                │ I/O, Channel, Mutex, Sleep            │
                ▼                                       │
           ┌──────────┐                                 │
           │ Waiting  │─────────────────────────────────┘
           └────┬─────┘     Event fires
                │
                │ Returns / Panics
                ▼
           ┌──────────┐
           │   Dead   │
           └──────────┘
```

### When goroutines yield (scheduling points)

The Go scheduler is mostly cooperative. Goroutines yield at:
- Channel operations (send/receive)
- `select` statements
- `sync.Mutex` lock acquisition
- System calls (I/O, sleep)
- Function calls (Go 1.14+ also has async preemption)
- Explicit `runtime.Gosched()`

---

## 4. Idiomatic Usage

### When to use goroutines

✅ **Use goroutines for:**
- Handling concurrent requests (HTTP, gRPC, etc.)
- Background tasks (periodic cleanup, metrics reporting)
- Parallel computation (when CPU-bound work can be split)
- I/O-bound operations that can overlap (multiple network calls)

❌ **Do NOT use goroutines for:**
- Sequential work that must happen in order
- Trivially fast operations (goroutine overhead > work done)
- As a replacement for function calls
- When you can't define a clear lifetime/termination condition

### Canonical patterns

**Pattern 1: Fire-and-forget with care**

```go
// BAD: No way to know when it finishes, no error handling
go processRequest(req)

// GOOD: Track completion, handle errors
go func() {
    if err := processRequest(req); err != nil {
        log.Printf("request failed: %v", err)
    }
}()
```

**Pattern 2: Goroutine per connection (from your routermgr)**

```go
func StartGrpcServer() {
    lis, err := net.Listen("tcp", GrpcPort)
    if err != nil {
        log.Fatalf("failed to listen: %v", err)
    }
    
    grpcServer := grpc.NewServer()
    routermgrpb.RegisterRouterMgrServer(grpcServer, &server{})
    
    // grpc.Server internally spawns a goroutine per connection
    // This is the standard pattern for network servers
    grpcServer.Serve(lis)  // Blocks, handles concurrency internally
}
```

**Pattern 3: Worker with context cancellation**

```go
func worker(ctx context.Context, jobs <-chan Job) {
    for {
        select {
        case <-ctx.Done():
            return  // Clean shutdown
        case job := <-jobs:
            process(job)
        }
    }
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Goroutine leak (most common bug)

```go
// LEAK: This goroutine runs forever if nobody reads from ch
func leaky() chan int {
    ch := make(chan int)
    go func() {
        for i := 0; ; i++ {
            ch <- i  // Blocks forever if receiver goes away
        }
    }()
    return ch
}
```

**Fix: Always provide a cancellation path**

```go
func notLeaky(ctx context.Context) chan int {
    ch := make(chan int)
    go func() {
        defer close(ch)
        for i := 0; ; i++ {
            select {
            case <-ctx.Done():
                return
            case ch <- i:
            }
        }
    }()
    return ch
}
```

### Pitfall 2: Loop variable capture (classic Go trap)

```go
// BUG: All goroutines see the SAME variable (the last value)
for _, route := range routes {
    go func() {
        processRoute(route)  // route is captured by reference!
    }()
}

// FIX 1: Pass as argument (argument is copied)
for _, route := range routes {
    go func(r Route) {
        processRoute(r)
    }(route)
}

// FIX 2: Shadow the variable (Go 1.22+ fixes this by default)
for _, route := range routes {
    route := route  // Creates new variable per iteration
    go func() {
        processRoute(route)
    }()
}
```

### Pitfall 3: No synchronization on main exit

```go
func main() {
    go doWork()  // May never run!
}  // Program exits immediately

// FIX: Wait for goroutine to complete
func main() {
    var wg sync.WaitGroup
    wg.Add(1)
    go func() {
        defer wg.Done()
        doWork()
    }()
    wg.Wait()
}
```

### Pitfall 4: Panic in goroutine kills everything

```go
// A panic in this goroutine will crash the entire program
go func() {
    panic("oops")  // Unrecovered panic = program crash
}()

// FIX: Contain panics at goroutine boundaries
go func() {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("goroutine panic: %v\n%s", r, debug.Stack())
        }
    }()
    riskyOperation()
}()
```

### Pitfall 5: Unbounded goroutine creation

```go
// Under load, this creates unlimited goroutines
func handleRequests(requests <-chan Request) {
    for req := range requests {
        go process(req)  // 1M requests = 1M goroutines = OOM
    }
}

// FIX: Worker pool pattern
func handleRequests(requests <-chan Request, workers int) {
    for i := 0; i < workers; i++ {
        go func() {
            for req := range requests {
                process(req)
            }
        }()
    }
}
```

---

## 6. Complete, Realistic Example

This example shows proper goroutine lifecycle management in a gRPC-style server:

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "net"
    "os"
    "os/signal"
    "sync"
    "syscall"
    "time"
)

// RouteUpdate represents a route change notification
type RouteUpdate struct {
    VrfID     uint32
    Prefix    string
    NextHop   string
    Operation string // "add" or "delete"
}

// RouteManager handles concurrent route updates with proper lifecycle
type RouteManager struct {
    updates    chan RouteUpdate
    done       chan struct{}
    wg         sync.WaitGroup
    mu         sync.RWMutex
    routes     map[string]RouteUpdate
    workerCount int
}

// NewRouteManager creates a route manager with controlled concurrency
func NewRouteManager(workerCount, bufferSize int) *RouteManager {
    return &RouteManager{
        updates:     make(chan RouteUpdate, bufferSize),
        done:        make(chan struct{}),
        routes:      make(map[string]RouteUpdate),
        workerCount: workerCount,
    }
}

// Start launches worker goroutines with proper lifecycle management
func (rm *RouteManager) Start(ctx context.Context) {
    // Launch worker pool - bounded concurrency
    for i := 0; i < rm.workerCount; i++ {
        rm.wg.Add(1)
        go rm.worker(ctx, i)
    }
    
    // Launch background cleanup goroutine
    rm.wg.Add(1)
    go rm.periodicCleanup(ctx)
    
    log.Printf("RouteManager started with %d workers", rm.workerCount)
}

// worker processes route updates - demonstrates proper goroutine pattern
func (rm *RouteManager) worker(ctx context.Context, id int) {
    defer rm.wg.Done()
    
    // Panic containment at goroutine boundary
    defer func() {
        if r := recover(); r != nil {
            log.Printf("worker %d panic recovered: %v", id, r)
        }
    }()
    
    for {
        select {
        case <-ctx.Done():
            log.Printf("worker %d shutting down: %v", id, ctx.Err())
            return
            
        case <-rm.done:
            log.Printf("worker %d received shutdown signal", id)
            return
            
        case update, ok := <-rm.updates:
            if !ok {
                // Channel closed, clean exit
                return
            }
            
            if err := rm.processUpdate(update); err != nil {
                log.Printf("worker %d failed to process update: %v", id, err)
                // Don't return - continue processing other updates
            }
        }
    }
}

// processUpdate applies a single route update
func (rm *RouteManager) processUpdate(update RouteUpdate) error {
    // Simulate network/system call
    time.Sleep(10 * time.Millisecond)
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", update.VrfID, update.Prefix)
    
    switch update.Operation {
    case "add":
        rm.routes[key] = update
        log.Printf("Added route: VRF=%d, %s via %s", 
            update.VrfID, update.Prefix, update.NextHop)
    case "delete":
        delete(rm.routes, key)
        log.Printf("Deleted route: VRF=%d, %s", update.VrfID, update.Prefix)
    default:
        return fmt.Errorf("unknown operation: %s", update.Operation)
    }
    
    return nil
}

// periodicCleanup demonstrates background goroutine with ticker
func (rm *RouteManager) periodicCleanup(ctx context.Context) {
    defer rm.wg.Done()
    
    ticker := time.NewTicker(30 * time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-ctx.Done():
            return
        case <-rm.done:
            return
        case <-ticker.C:
            rm.cleanup()
        }
    }
}

func (rm *RouteManager) cleanup() {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    log.Printf("Cleanup: currently tracking %d routes", len(rm.routes))
}

// SubmitUpdate queues an update - non-blocking with backpressure
func (rm *RouteManager) SubmitUpdate(ctx context.Context, update RouteUpdate) error {
    select {
    case <-ctx.Done():
        return ctx.Err()
    case rm.updates <- update:
        return nil
    default:
        // Buffer full - apply backpressure
        return errors.New("update buffer full, try again later")
    }
}

// Shutdown gracefully stops all goroutines
func (rm *RouteManager) Shutdown(timeout time.Duration) error {
    log.Println("Initiating graceful shutdown...")
    
    // Signal workers to stop
    close(rm.done)
    
    // Wait for workers with timeout
    done := make(chan struct{})
    go func() {
        rm.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("All workers stopped gracefully")
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timed out, some goroutines may be leaked")
    }
}

// Server demonstrates a complete concurrent server
type Server struct {
    listener     net.Listener
    routeManager *RouteManager
    ctx          context.Context
    cancel       context.CancelFunc
}

func NewServer(addr string) (*Server, error) {
    lis, err := net.Listen("tcp", addr)
    if err != nil {
        return nil, fmt.Errorf("failed to listen: %w", err)
    }
    
    ctx, cancel := context.WithCancel(context.Background())
    
    return &Server{
        listener:     lis,
        routeManager: NewRouteManager(4, 1000),  // 4 workers, 1000 buffer
        ctx:          ctx,
        cancel:       cancel,
    }, nil
}

func (s *Server) Start() {
    s.routeManager.Start(s.ctx)
    
    // Accept connections in goroutine
    go s.acceptLoop()
    
    log.Printf("Server listening on %s", s.listener.Addr())
}

func (s *Server) acceptLoop() {
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            select {
            case <-s.ctx.Done():
                return  // Expected during shutdown
            default:
                log.Printf("Accept error: %v", err)
                continue
            }
        }
        
        // Handle each connection in its own goroutine
        go s.handleConnection(conn)
    }
}

func (s *Server) handleConnection(conn net.Conn) {
    defer conn.Close()
    
    // Connection-scoped context
    ctx, cancel := context.WithTimeout(s.ctx, 30*time.Second)
    defer cancel()
    
    // Simulate processing
    update := RouteUpdate{
        VrfID:     1,
        Prefix:    "10.0.0.0/24",
        NextHop:   "192.168.1.1",
        Operation: "add",
    }
    
    if err := s.routeManager.SubmitUpdate(ctx, update); err != nil {
        log.Printf("Failed to submit update: %v", err)
        return
    }
    
    conn.Write([]byte("OK\n"))
}

func (s *Server) Shutdown() error {
    s.cancel()  // Cancel context to stop all goroutines
    s.listener.Close()
    return s.routeManager.Shutdown(5 * time.Second)
}

func main() {
    server, err := NewServer(":8080")
    if err != nil {
        log.Fatal(err)
    }
    
    server.Start()
    
    // Wait for interrupt signal
    sigCh := make(chan os.Signal, 1)
    signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
    
    sig := <-sigCh
    log.Printf("Received signal %v, shutting down...", sig)
    
    if err := server.Shutdown(); err != nil {
        log.Printf("Shutdown error: %v", err)
        os.Exit(1)
    }
    
    log.Println("Server stopped")
}
```

---

## 7. Design Takeaways

### Rules of thumb

1. **Every goroutine must have a clear termination condition**
   - Use `context.Context` for cancellation
   - Provide a `done` channel as backup
   - Never start a goroutine you can't stop

2. **Bound your concurrency**
   - Use worker pools, not unbounded `go` statements
   - Define buffer sizes explicitly
   - Plan for backpressure

3. **Own the panic boundary**
   - Recover at goroutine entry points
   - Log with stack traces
   - Let the goroutine die gracefully, not silently

4. **Think about what happens during shutdown**
   - How long do you wait for goroutines?
   - What happens to in-flight work?
   - Are there resources that need cleanup?

5. **Goroutines are cheap, but not free**
   - Each costs ~2KB minimum (can grow to MBs)
   - Scheduling has overhead
   - Track your goroutine count in production

### How this affects system design

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GOROUTINE-AWARE ARCHITECTURE                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Server Component Design:                                              │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  ┌─────────────┐      ┌─────────────┐      ┌─────────────────┐  │   │
│   │  │   Accept    │      │   Worker    │      │   Background    │  │   │
│   │  │   Loop      │      │   Pool      │      │   Tasks         │  │   │
│   │  │  (1 gorout) │      │ (N gorouts) │      │  (M gorouts)    │  │   │
│   │  └──────┬──────┘      └──────▲──────┘      └────────▲────────┘  │   │
│   │         │                    │                      │           │   │
│   │         │  conn              │ jobs                 │ ctx.Done  │   │
│   │         ▼                    │                      │           │   │
│   │  ┌─────────────────────────────────────────────────────────────┐│   │
│   │  │                   Buffered Channels                         ││   │
│   │  │              (backpressure, bounded memory)                 ││   │
│   │  └─────────────────────────────────────────────────────────────┘│   │
│   │                                                                 │   │
│   │  Lifecycle: ctx.Context ──────────────► cancellation ──► wg.Wait│   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│   Key Decisions:                                                        │
│   • Worker count = GOMAXPROCS or tuned based on I/O vs CPU              │
│   • Buffer size = latency tolerance × throughput                        │
│   • Shutdown timeout = max acceptable in-flight work duration           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Goroutine 核心概念

**Goroutine 不是线程，而是由 Go 运行时管理的并发执行单元。**

1. **轻量级**：初始栈仅 2KB，可按需增长；OS 线程通常需要 1-8MB
2. **M:N 调度**：多个 Goroutine 复用到少量 OS 线程上执行
3. **协作式调度**：Goroutine 在特定点（如 channel 操作、系统调用）让出执行权
4. **阻塞不昂贵**：一个 Goroutine 阻塞不会阻塞底层 OS 线程

### 关键设计原则

1. **每个 Goroutine 必须有明确的终止条件**
   - 使用 `context.Context` 传播取消信号
   - 永远不要启动无法停止的 Goroutine

2. **限制并发度**
   - 使用工作池模式，不要无限制地启动 Goroutine
   - 明确定义缓冲区大小
   - 设计背压机制

3. **在 Goroutine 边界处理 panic**
   - 使用 `recover()` 捕获 panic
   - 记录日志和堆栈信息
   - 让 Goroutine 优雅退出

4. **考虑关闭时的行为**
   - 等待多长时间？
   - 正在处理的工作怎么办？
   - 是否有资源需要清理？

### 常见错误

1. **Goroutine 泄漏**：启动了无法终止的 Goroutine，内存持续增长
2. **循环变量捕获**：闭包捕获了循环变量的引用，导致所有 Goroutine 看到相同的值
3. **主函数过早退出**：main 函数结束时，所有 Goroutine 都会被终止
4. **未处理的 panic**：Goroutine 中的 panic 会导致整个程序崩溃
5. **无限制创建 Goroutine**：高负载下可能导致内存耗尽

