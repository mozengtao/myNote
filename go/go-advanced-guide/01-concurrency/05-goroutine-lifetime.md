# Goroutine Lifetime Management

## 1. Engineering Problem

### What real-world problem does this solve?

**Every goroutine you start is your responsibility to stop.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE GOROUTINE LIFECYCLE PROBLEM                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Start a goroutine:    Easy                                            │
│   ──────────────────    go doWork()                                     │
│                                                                         │
│   Stop a goroutine:     Hard                                            │
│   ─────────────────     • How does the goroutine know to stop?          │
│                         • How do you wait for it to finish?             │
│                         • How do you handle in-flight work?             │
│                         • What if it's blocked on I/O?                  │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Without lifecycle management:                                         │
│                                                                         │
│   main() ─────────────────────────────────────────────────►exit         │
│           │                                                             │
│           │ go worker()                                                 │
│           └────────────────────────────────────────►leaked              │
│                    No cleanup, no graceful shutdown                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   With proper lifecycle management:                                     │
│                                                                         │
│   main() ──────────┬───────────────────┬────────────────►exit           │
│                    │                   │ wg.Wait()                      │
│           go worker(ctx)               │                                │
│                    │                   │                                │
│                    └─────<-ctx.Done()──┴► return (clean)                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong without lifecycle management?

1. **Goroutine leaks**: Memory grows indefinitely
2. **Orphaned resources**: Database connections, file handles left open
3. **Incomplete work**: Writes not flushed, transactions not committed
4. **Testing failures**: Tests can't verify cleanup happened
5. **Unpredictable shutdown**: Random failures on process exit

---

## 2. Core Mental Model

### The Lifecycle Contract

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GOROUTINE LIFECYCLE CONTRACT                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   When you start a goroutine, you must answer:                          │
│                                                                         │
│   1. HOW will it know to stop?                                          │
│      • Context cancellation (ctx.Done())                                │
│      • Closing its input channel                                        │
│      • Explicit done channel                                            │
│                                                                         │
│   2. HOW will you wait for it to finish?                                │
│      • sync.WaitGroup                                                   │
│      • Reading from done channel                                        │
│      • errgroup                                                         │
│                                                                         │
│   3. WHAT cleanup does it need to do?                                   │
│      • Flush buffers                                                    │
│      • Close connections                                                │
│      • Release locks                                                    │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Hierarchy:                                                            │
│                                                                         │
│   main() ◄─────── owns ───────► Server                                  │
│                                    │                                    │
│                                    ├── owns ──► Worker pool             │
│                                    │               ├── Worker 1         │
│                                    │               ├── Worker 2         │
│                                    │               └── Worker N         │
│                                    │                                    │
│                                    └── owns ──► Background tasks        │
│                                                    ├── Metrics          │
│                                                    └── Cleanup          │
│                                                                         │
│   Shutdown flows top-down: main cancels → Server stops → Workers stop   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Ownership Model

- **Owner**: The code that starts a goroutine
- **Owned**: The goroutine being started
- **Contract**: Owner provides cancellation signal; owned respects it

---

## 3. Language Mechanism

### Pattern 1: Context-based lifecycle

```go
func worker(ctx context.Context) {
    for {
        select {
        case <-ctx.Done():
            // Cleanup
            return
        default:
            doWork()
        }
    }
}

// Start
ctx, cancel := context.WithCancel(context.Background())
go worker(ctx)

// Stop
cancel()
```

### Pattern 2: WaitGroup for completion tracking

```go
var wg sync.WaitGroup

wg.Add(1)
go func() {
    defer wg.Done()
    doWork()
}()

// Wait for completion
wg.Wait()
```

### Pattern 3: Combined context + WaitGroup

```go
type Manager struct {
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
}

func (m *Manager) StartWorker() {
    m.wg.Add(1)
    go func() {
        defer m.wg.Done()
        
        for {
            select {
            case <-m.ctx.Done():
                return
            default:
                doWork()
            }
        }
    }()
}

func (m *Manager) Shutdown() {
    m.cancel()      // Signal stop
    m.wg.Wait()     // Wait for completion
}
```

### Pattern 4: done channel

```go
func worker(done <-chan struct{}) {
    for {
        select {
        case <-done:
            return
        default:
            doWork()
        }
    }
}

// Start
done := make(chan struct{})
go worker(done)

// Stop (broadcasts to all workers reading done)
close(done)
```

---

## 4. Idiomatic Usage

### The canonical server pattern

```go
type Server struct {
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
    
    listener net.Listener
}

func NewServer(addr string) (*Server, error) {
    lis, err := net.Listen("tcp", addr)
    if err != nil {
        return nil, err
    }
    
    ctx, cancel := context.WithCancel(context.Background())
    
    return &Server{
        ctx:      ctx,
        cancel:   cancel,
        listener: lis,
    }, nil
}

func (s *Server) Start() {
    // Accept loop
    s.wg.Add(1)
    go s.acceptLoop()
    
    // Background tasks
    s.wg.Add(1)
    go s.metricsReporter()
}

func (s *Server) acceptLoop() {
    defer s.wg.Done()
    
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            select {
            case <-s.ctx.Done():
                return  // Expected during shutdown
            default:
                log.Printf("accept error: %v", err)
                continue
            }
        }
        
        s.wg.Add(1)
        go s.handleConnection(conn)
    }
}

func (s *Server) handleConnection(conn net.Conn) {
    defer s.wg.Done()
    defer conn.Close()
    
    // Connection-scoped context
    ctx, cancel := context.WithCancel(s.ctx)
    defer cancel()
    
    // Handle connection...
    _ = ctx
}

func (s *Server) metricsReporter() {
    defer s.wg.Done()
    
    ticker := time.NewTicker(time.Minute)
    defer ticker.Stop()
    
    for {
        select {
        case <-s.ctx.Done():
            return
        case <-ticker.C:
            reportMetrics()
        }
    }
}

func (s *Server) Shutdown(timeout time.Duration) error {
    // 1. Stop accepting new connections
    s.listener.Close()
    
    // 2. Signal all goroutines to stop
    s.cancel()
    
    // 3. Wait with timeout
    done := make(chan struct{})
    go func() {
        s.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timeout")
    }
}
```

### Using errgroup for error propagation

```go
import "golang.org/x/sync/errgroup"

func runWorkers(ctx context.Context) error {
    g, ctx := errgroup.WithContext(ctx)
    
    for i := 0; i < 3; i++ {
        i := i
        g.Go(func() error {
            return worker(ctx, i)
        })
    }
    
    // Wait for all workers, return first error
    return g.Wait()
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Forgetting to wait

```go
// BAD: main exits before goroutine completes
func main() {
    go longRunningTask()
}  // Program exits, goroutine killed mid-work

// GOOD: Wait for completion
func main() {
    var wg sync.WaitGroup
    wg.Add(1)
    go func() {
        defer wg.Done()
        longRunningTask()
    }()
    wg.Wait()
}
```

### Pitfall 2: No cancellation path

```go
// BAD: No way to stop this worker
func worker(jobs <-chan Job) {
    for job := range jobs {  // Waits forever if channel not closed
        process(job)
    }
}

// GOOD: Check for cancellation
func worker(ctx context.Context, jobs <-chan Job) {
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
}
```

### Pitfall 3: Blocking cleanup

```go
// BAD: Cleanup can block indefinitely
func worker(ctx context.Context, out chan<- Result) {
    defer func() {
        out <- Result{Final: true}  // Blocks if nobody reading!
    }()
    
    for {
        select {
        case <-ctx.Done():
            return
        default:
            doWork()
        }
    }
}

// GOOD: Non-blocking cleanup
func worker(ctx context.Context, out chan<- Result) {
    defer func() {
        select {
        case out <- Result{Final: true}:
        default:
            // Skip if channel full
        }
    }()
    // ...
}
```

### Pitfall 4: WaitGroup misuse

```go
// BUG: Adding to WaitGroup after Wait started
var wg sync.WaitGroup

go func() {
    for task := range tasks {
        wg.Add(1)  // Race condition! Wait might be running
        go func(t Task) {
            defer wg.Done()
            process(t)
        }(task)
    }
}()

wg.Wait()  // Might return too early

// FIX: Add before goroutine
for task := range tasks {
    wg.Add(1)
    go func(t Task) {
        defer wg.Done()
        process(t)
    }(task)
}
wg.Wait()
```

### Pitfall 5: Ignoring shutdown deadline

```go
// BAD: Waits forever
func (s *Server) Shutdown() {
    s.cancel()
    s.wg.Wait()  // What if a worker is stuck?
}

// GOOD: Timeout on shutdown
func (s *Server) Shutdown(timeout time.Duration) error {
    s.cancel()
    
    done := make(chan struct{})
    go func() {
        s.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timeout: some goroutines still running")
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
    "net"
    "os"
    "os/signal"
    "sync"
    "syscall"
    "time"
)

// RouteManager demonstrates proper lifecycle management
// Based on the routermgr pattern
type RouteManager struct {
    // Lifecycle control
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
    
    // Components
    listener     net.Listener
    updates      chan RouteUpdate
    workerCount  int
    
    // State
    mu     sync.RWMutex
    routes map[string]Route
    
    // Metrics
    processed  int64
    errors     int64
    startTime  time.Time
}

type RouteUpdate struct {
    Operation string
    Route     Route
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteManager(addr string, workers int) (*RouteManager, error) {
    lis, err := net.Listen("tcp", addr)
    if err != nil {
        return nil, fmt.Errorf("listen: %w", err)
    }
    
    ctx, cancel := context.WithCancel(context.Background())
    
    return &RouteManager{
        ctx:         ctx,
        cancel:      cancel,
        listener:    lis,
        updates:     make(chan RouteUpdate, 100),
        workerCount: workers,
        routes:      make(map[string]Route),
        startTime:   time.Now(),
    }, nil
}

// Start begins all goroutines with proper lifecycle tracking
func (rm *RouteManager) Start() {
    log.Printf("RouteManager starting with %d workers", rm.workerCount)
    
    // 1. Start workers first (consumers)
    for i := 0; i < rm.workerCount; i++ {
        rm.wg.Add(1)
        go rm.worker(i)
    }
    
    // 2. Start accept loop (producer)
    rm.wg.Add(1)
    go rm.acceptLoop()
    
    // 3. Start background tasks
    rm.wg.Add(1)
    go rm.periodicCleanup()
    
    rm.wg.Add(1)
    go rm.metricsReporter()
    
    log.Println("RouteManager started")
}

func (rm *RouteManager) acceptLoop() {
    defer rm.wg.Done()
    defer log.Println("Accept loop stopped")
    
    for {
        conn, err := rm.listener.Accept()
        if err != nil {
            select {
            case <-rm.ctx.Done():
                return  // Expected during shutdown
            default:
                log.Printf("Accept error: %v", err)
                continue
            }
        }
        
        // Each connection gets its own goroutine
        rm.wg.Add(1)
        go rm.handleConnection(conn)
    }
}

func (rm *RouteManager) handleConnection(conn net.Conn) {
    defer rm.wg.Done()
    defer conn.Close()
    
    // Connection-scoped context with timeout
    ctx, cancel := context.WithTimeout(rm.ctx, 30*time.Second)
    defer cancel()
    
    // Set read deadline
    conn.SetReadDeadline(time.Now().Add(10 * time.Second))
    
    buf := make([]byte, 1024)
    n, err := conn.Read(buf)
    if err != nil {
        if ctx.Err() != nil {
            return  // Context cancelled
        }
        log.Printf("Read error: %v", err)
        return
    }
    
    // Parse and queue update
    update := parseUpdate(buf[:n])
    
    select {
    case <-ctx.Done():
        return
    case rm.updates <- update:
        conn.Write([]byte("OK\n"))
    case <-time.After(time.Second):
        conn.Write([]byte("BUSY\n"))
    }
}

func parseUpdate(data []byte) RouteUpdate {
    // Simplified parsing
    return RouteUpdate{
        Operation: "add",
        Route: Route{
            VrfID:   1,
            Prefix:  string(data),
            NextHop: "10.0.0.1",
        },
    }
}

func (rm *RouteManager) worker(id int) {
    defer rm.wg.Done()
    defer log.Printf("Worker %d stopped", id)
    
    log.Printf("Worker %d started", id)
    
    for {
        select {
        case <-rm.ctx.Done():
            return
            
        case update, ok := <-rm.updates:
            if !ok {
                return  // Channel closed
            }
            
            if err := rm.applyUpdate(update); err != nil {
                log.Printf("Worker %d: failed to apply update: %v", id, err)
                rm.errors++
            } else {
                rm.processed++
            }
        }
    }
}

func (rm *RouteManager) applyUpdate(update RouteUpdate) error {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", update.Route.VrfID, update.Route.Prefix)
    
    switch update.Operation {
    case "add":
        rm.routes[key] = update.Route
    case "delete":
        delete(rm.routes, key)
    default:
        return fmt.Errorf("unknown operation: %s", update.Operation)
    }
    
    return nil
}

func (rm *RouteManager) periodicCleanup() {
    defer rm.wg.Done()
    defer log.Println("Cleanup task stopped")
    
    ticker := time.NewTicker(time.Minute)
    defer ticker.Stop()
    
    for {
        select {
        case <-rm.ctx.Done():
            return
        case <-ticker.C:
            rm.cleanup()
        }
    }
}

func (rm *RouteManager) cleanup() {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    log.Printf("Cleanup: %d routes in table", len(rm.routes))
}

func (rm *RouteManager) metricsReporter() {
    defer rm.wg.Done()
    defer log.Println("Metrics reporter stopped")
    
    ticker := time.NewTicker(10 * time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-rm.ctx.Done():
            return
        case <-ticker.C:
            uptime := time.Since(rm.startTime)
            log.Printf("Metrics: uptime=%v processed=%d errors=%d routes=%d",
                uptime, rm.processed, rm.errors, len(rm.routes))
        }
    }
}

// Shutdown performs graceful shutdown with timeout
func (rm *RouteManager) Shutdown(timeout time.Duration) error {
    log.Println("Initiating shutdown...")
    
    // Phase 1: Stop accepting new connections
    rm.listener.Close()
    
    // Phase 2: Close updates channel (workers will drain and exit)
    close(rm.updates)
    
    // Phase 3: Cancel context (signals all goroutines)
    rm.cancel()
    
    // Phase 4: Wait with timeout
    done := make(chan struct{})
    go func() {
        rm.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("Shutdown complete")
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timeout: some goroutines still running")
    }
}

// GetStatus returns current status (thread-safe)
func (rm *RouteManager) GetStatus() map[string]interface{} {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    return map[string]interface{}{
        "uptime":     time.Since(rm.startTime).String(),
        "routes":     len(rm.routes),
        "processed":  rm.processed,
        "errors":     rm.errors,
    }
}

func main() {
    // Create manager
    manager, err := NewRouteManager(":8080", 4)
    if err != nil {
        log.Fatal(err)
    }
    
    // Start manager
    manager.Start()
    
    // Setup signal handling
    sigCh := make(chan os.Signal, 1)
    signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
    
    // Wait for signal
    sig := <-sigCh
    log.Printf("Received signal: %v", sig)
    
    // Graceful shutdown
    if err := manager.Shutdown(10 * time.Second); err != nil {
        log.Printf("Shutdown error: %v", err)
        os.Exit(1)
    }
    
    fmt.Println("Status:", manager.GetStatus())
}
```

---

## 7. Design Takeaways

### The Lifecycle Checklist

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 GOROUTINE LIFECYCLE CHECKLIST                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   For every goroutine you start, verify:                                │
│                                                                         │
│   □ How does it know to stop? (ctx.Done, closed channel, done signal)   │
│   □ How do you wait for it? (WaitGroup, errgroup, done channel)         │
│   □ What cleanup does it need? (flush, close, release)                  │
│   □ What happens if cleanup blocks?                                     │
│   □ What's the shutdown timeout?                                        │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Shutdown order:                                                       │
│                                                                         │
│   1. Stop accepting new work (close listener, close input channels)     │
│   2. Signal workers to stop (cancel context)                            │
│   3. Wait for workers to drain (with timeout)                           │
│   4. Force kill if timeout exceeded                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Testing lifecycle:                                                    │
│                                                                         │
│   • Test that shutdown completes within timeout                         │
│   • Test that in-flight work is handled                                 │
│   • Test behavior under load during shutdown                            │
│   • Verify no goroutine leaks (runtime.NumGoroutine)                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Goroutine 生命周期管理核心概念

**启动 goroutine 容易，管理其生命周期才是关键。**

### 必须回答的三个问题

每次启动 goroutine 时必须明确：

1. **如何通知它停止？**
   - `context.Context` 的 `Done()` 方法
   - 关闭输入 channel
   - 显式的 done channel

2. **如何等待它完成？**
   - `sync.WaitGroup`
   - `errgroup`
   - done channel

3. **它需要做什么清理？**
   - 刷新缓冲区
   - 关闭连接
   - 释放锁

### 规范的服务器模式

```go
type Server struct {
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
}

func (s *Server) Shutdown(timeout time.Duration) error {
    s.cancel()      // 发送停止信号
    
    done := make(chan struct{})
    go func() {
        s.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timeout")
    }
}
```

### 关闭顺序

1. 停止接受新工作（关闭 listener，关闭输入 channel）
2. 发送停止信号（取消 context）
3. 等待 worker 排空（带超时）
4. 超时则强制终止

### 常见错误

1. **忘记等待**：main 退出时 goroutine 被杀死
2. **没有取消路径**：goroutine 永远无法停止
3. **阻塞的清理代码**：defer 中的操作可能阻塞
4. **WaitGroup 误用**：在 Wait 之后调用 Add
5. **忽略关闭超时**：无限等待可能导致进程挂起

