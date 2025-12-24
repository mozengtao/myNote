# sync Primitives: Mutex, WaitGroup, Once, and More

## 1. Engineering Problem

### What real-world problem does this solve?

**Channels are great, but sometimes shared memory is the right tool.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    WHEN TO USE SYNC VS CHANNELS                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Use CHANNELS when:                                                    │
│   ─────────────────                                                     │
│   • Passing ownership of data                                           │
│   • Coordinating goroutines                                             │
│   • Distributing work                                                   │
│   • Signaling events                                                    │
│                                                                         │
│   Use SYNC primitives when:                                             │
│   ─────────────────────────                                             │
│   • Protecting shared state (caches, counters, maps)                    │
│   • Simple mutual exclusion                                             │
│   • Waiting for multiple goroutines (WaitGroup)                         │
│   • One-time initialization (Once)                                      │
│   • Object pools (Pool)                                                 │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Example from routermgr:                                               │
│                                                                         │
│   var addressMutex sync.Mutex                                           │
│   var routeMutex   sync.Mutex                                           │
│                                                                         │
│   These protect maps like RouterAddresses and VmcRoutes                 │
│   from concurrent access - much simpler than channel-based approach     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when engineers misunderstand sync primitives?

1. **Deadlocks**: Acquiring locks in inconsistent order
2. **Race conditions**: Forgetting to lock in one place
3. **Performance**: Lock contention under high load
4. **Copy bugs**: Copying sync types (they contain internal state)
5. **WaitGroup misuse**: Adding after Wait() started

---

## 2. Core Mental Model

### The sync package overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SYNC PACKAGE OVERVIEW                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Mutual Exclusion:                                                     │
│   ─────────────────                                                     │
│   sync.Mutex       • Simple lock/unlock                                 │
│   sync.RWMutex     • Multiple readers OR one writer                     │
│                                                                         │
│   Coordination:                                                         │
│   ─────────────                                                         │
│   sync.WaitGroup   • Wait for N goroutines to complete                  │
│   sync.Cond        • Signal waiting goroutines (rare)                   │
│                                                                         │
│   One-time Initialization:                                              │
│   ────────────────────────                                              │
│   sync.Once        • Run function exactly once                          │
│                                                                         │
│   Object Reuse:                                                         │
│   ─────────────                                                         │
│   sync.Pool        • Cache of temporary objects                         │
│                                                                         │
│   Atomic Values:                                                        │
│   ──────────────                                                        │
│   sync.Map         • Concurrent-safe map (specific use cases)           │
│   sync/atomic      • Low-level atomic operations                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. sync.Mutex

### Basic usage

```go
type RouteManager struct {
    mu     sync.Mutex  // Protects routes
    routes map[string]Route
}

func (rm *RouteManager) AddRoute(key string, r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()  // Always defer unlock
    rm.routes[key] = r
}

func (rm *RouteManager) GetRoute(key string) (Route, bool) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    r, ok := rm.routes[key]
    return r, ok
}
```

### Critical rules

```go
// Rule 1: Always use defer for unlock
func good() {
    mu.Lock()
    defer mu.Unlock()  // Guaranteed even on panic
    // ... work ...
}

// Rule 2: Never copy a Mutex
type Bad struct {
    mu sync.Mutex
}

b1 := Bad{}
b2 := b1  // BUG: Copies the mutex!

// Rule 3: Document what the mutex protects
type Good struct {
    mu      sync.Mutex  // Protects routes and version
    routes  map[string]Route
    version int
}
```

### Lock ordering to prevent deadlocks

```go
// DEADLOCK: Inconsistent lock order
// Goroutine 1: Lock A, then Lock B
// Goroutine 2: Lock B, then Lock A

// FIX: Always acquire locks in consistent order
type Manager struct {
    muA sync.Mutex  // Always lock before muB
    muB sync.Mutex
}

func (m *Manager) SafeOperation() {
    m.muA.Lock()
    defer m.muA.Unlock()
    m.muB.Lock()
    defer m.muB.Unlock()
    // work
}
```

---

## 4. sync.RWMutex

### When reads dominate writes

```go
type Cache struct {
    mu    sync.RWMutex  // Multiple readers, single writer
    items map[string]string
}

// Multiple goroutines can read simultaneously
func (c *Cache) Get(key string) (string, bool) {
    c.mu.RLock()         // Read lock
    defer c.mu.RUnlock()
    v, ok := c.items[key]
    return v, ok
}

// Only one writer at a time (blocks all readers too)
func (c *Cache) Set(key, value string) {
    c.mu.Lock()         // Write lock
    defer c.mu.Unlock()
    c.items[key] = value
}
```

### When to use RWMutex vs Mutex

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MUTEX vs RWMUTEX DECISION                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Use sync.Mutex when:                                                  │
│   • Reads and writes are roughly equal                                  │
│   • Critical section is very short                                      │
│   • Simplicity matters more than optimization                           │
│                                                                         │
│   Use sync.RWMutex when:                                                │
│   • Reads VASTLY outnumber writes (10:1 or more)                       │
│   • Critical section has meaningful duration                            │
│   • You've profiled and confirmed contention                            │
│                                                                         │
│   Note: RWMutex has higher overhead than Mutex                          │
│   Don't use RWMutex "just in case" - profile first                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. sync.WaitGroup

### Waiting for goroutines

```go
func processItems(items []Item) {
    var wg sync.WaitGroup
    
    for _, item := range items {
        wg.Add(1)  // Add BEFORE starting goroutine
        go func(it Item) {
            defer wg.Done()
            process(it)
        }(item)
    }
    
    wg.Wait()  // Block until all Done() called
    fmt.Println("All items processed")
}
```

### Critical WaitGroup rules

```go
// Rule 1: Add() before the goroutine starts
// WRONG
go func() {
    wg.Add(1)  // Race condition!
    defer wg.Done()
}()

// CORRECT
wg.Add(1)
go func() {
    defer wg.Done()
}()

// Rule 2: Don't Add() after Wait() might have started
// WRONG
go func() {
    for item := range items {
        wg.Add(1)  // Race if Wait() running
        go process(item)
    }
}()
wg.Wait()

// CORRECT: Know count upfront
wg.Add(len(items))
for _, item := range items {
    go func(it Item) {
        defer wg.Done()
        process(it)
    }(item)
}
wg.Wait()
```

---

## 6. sync.Once

### One-time initialization

```go
var (
    instance *Database
    once     sync.Once
)

func GetDatabase() *Database {
    once.Do(func() {
        // This runs exactly once, even if called concurrently
        instance = &Database{
            conn: connectToDatabase(),
        }
    })
    return instance
}
```

### Common patterns

```go
// Lazy initialization
type Config struct {
    once   sync.Once
    values map[string]string
}

func (c *Config) Load() {
    c.once.Do(func() {
        c.values = loadFromFile()
    })
}

func (c *Config) Get(key string) string {
    c.Load()  // Safe to call multiple times
    return c.values[key]
}

// Singleton with cleanup
type Service struct {
    initOnce    sync.Once
    cleanupOnce sync.Once
    conn        *Connection
}

func (s *Service) Init() {
    s.initOnce.Do(func() {
        s.conn = connect()
    })
}

func (s *Service) Cleanup() {
    s.cleanupOnce.Do(func() {
        s.conn.Close()
    })
}
```

---

## 7. sync.Pool

### Object reuse for performance

```go
var bufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}

func processRequest(data []byte) {
    // Get buffer from pool
    buf := bufferPool.Get().([]byte)
    
    // Reset before use (important!)
    buf = buf[:0]  // Reset slice to zero length
    
    // Use buffer...
    copy(buf, data)
    
    // Return to pool
    bufferPool.Put(buf)
}
```

### When to use sync.Pool

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYNC.POOL GUIDELINES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Use sync.Pool when:                                                   │
│   • High allocation rate of same-sized objects                          │
│   • Objects are expensive to create                                     │
│   • Profiling shows allocation is a bottleneck                          │
│                                                                         │
│   Don't use sync.Pool when:                                             │
│   • Objects are cheap to create                                         │
│   • You haven't profiled                                                │
│   • Objects need initialization state                                   │
│                                                                         │
│   Important:                                                            │
│   • Pool may be cleared during GC                                       │
│   • No guarantee of object reuse                                        │
│   • Must reset object state before returning to pool                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 8. sync/atomic

### Lock-free operations

```go
import "sync/atomic"

type Counter struct {
    value int64
}

func (c *Counter) Inc() {
    atomic.AddInt64(&c.value, 1)
}

func (c *Counter) Get() int64 {
    return atomic.LoadInt64(&c.value)
}

// Since Go 1.19: atomic types
type Counter2 struct {
    value atomic.Int64
}

func (c *Counter2) Inc() {
    c.value.Add(1)
}

func (c *Counter2) Get() int64 {
    return c.value.Load()
}
```

### When to use atomics vs mutex

```go
// Good for atomics: Simple counters, flags
var requestCount atomic.Int64
requestCount.Add(1)

// Use mutex instead: Multiple related fields
type Stats struct {
    mu       sync.Mutex
    requests int64
    errors   int64
    lastErr  error  // Related to errors count
}

func (s *Stats) RecordError(err error) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.errors++
    s.lastErr = err  // Must be consistent with errors count
}
```

---

## 9. Complete Example

```go
package main

import (
    "context"
    "fmt"
    "log"
    "sync"
    "sync/atomic"
    "time"
)

// RouteTable demonstrates proper use of sync primitives
type RouteTable struct {
    mu      sync.RWMutex  // Protects routes
    routes  map[string]Route
    version atomic.Int64
    
    once    sync.Once     // For lazy initialization
    metrics *Metrics
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type Metrics struct {
    gets    atomic.Int64
    sets    atomic.Int64
    deletes atomic.Int64
}

func NewRouteTable() *RouteTable {
    return &RouteTable{
        routes: make(map[string]Route),
    }
}

// Get uses read lock for concurrent access
func (rt *RouteTable) Get(key string) (Route, bool) {
    rt.mu.RLock()
    defer rt.mu.RUnlock()
    
    rt.getMetrics().gets.Add(1)
    r, ok := rt.routes[key]
    return r, ok
}

// Set uses write lock for exclusive access
func (rt *RouteTable) Set(key string, route Route) {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    rt.routes[key] = route
    rt.version.Add(1)
    rt.getMetrics().sets.Add(1)
}

// Delete uses write lock
func (rt *RouteTable) Delete(key string) bool {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    if _, ok := rt.routes[key]; ok {
        delete(rt.routes, key)
        rt.version.Add(1)
        rt.getMetrics().deletes.Add(1)
        return true
    }
    return false
}

// GetAll returns a copy (safe to iterate)
func (rt *RouteTable) GetAll() []Route {
    rt.mu.RLock()
    defer rt.mu.RUnlock()
    
    result := make([]Route, 0, len(rt.routes))
    for _, r := range rt.routes {
        result = append(result, r)
    }
    return result
}

// getMetrics uses Once for lazy initialization
func (rt *RouteTable) getMetrics() *Metrics {
    rt.once.Do(func() {
        rt.metrics = &Metrics{}
    })
    return rt.metrics
}

// Version returns current version (lock-free)
func (rt *RouteTable) Version() int64 {
    return rt.version.Load()
}

// Stats returns current statistics
func (rt *RouteTable) Stats() map[string]int64 {
    m := rt.getMetrics()
    return map[string]int64{
        "gets":    m.gets.Load(),
        "sets":    m.sets.Load(),
        "deletes": m.deletes.Load(),
        "version": rt.version.Load(),
    }
}

// BatchProcessor demonstrates WaitGroup usage
type BatchProcessor struct {
    table       *RouteTable
    workerCount int
}

func (bp *BatchProcessor) ProcessRoutes(ctx context.Context, routes []Route) error {
    var wg sync.WaitGroup
    errCh := make(chan error, len(routes))
    
    // Semaphore for bounded concurrency
    sem := make(chan struct{}, bp.workerCount)
    
    for _, route := range routes {
        select {
        case <-ctx.Done():
            break
        case sem <- struct{}{}:
        }
        
        wg.Add(1)
        go func(r Route) {
            defer wg.Done()
            defer func() { <-sem }()
            
            // Simulate processing
            time.Sleep(10 * time.Millisecond)
            
            key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
            bp.table.Set(key, r)
        }(route)
    }
    
    // Wait for all workers
    wg.Wait()
    close(errCh)
    
    // Collect errors
    for err := range errCh {
        if err != nil {
            return err
        }
    }
    
    return ctx.Err()
}

// BufferPool demonstrates sync.Pool usage
var bufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 0, 4096)
    },
}

func processWithBuffer(data []byte) []byte {
    buf := bufferPool.Get().([]byte)
    buf = buf[:0]  // Reset
    
    // Process data
    buf = append(buf, data...)
    buf = append(buf, '\n')
    
    // Make a copy for return (pool buffer will be reused)
    result := make([]byte, len(buf))
    copy(result, buf)
    
    bufferPool.Put(buf)
    return result
}

func main() {
    table := NewRouteTable()
    
    // Concurrent access demo
    var wg sync.WaitGroup
    
    // Writers
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                key := fmt.Sprintf("%d:10.%d.%d.0/24", id, id, j)
                table.Set(key, Route{
                    VrfID:   uint32(id),
                    Prefix:  fmt.Sprintf("10.%d.%d.0/24", id, j),
                    NextHop: "192.168.1.1",
                })
            }
        }(i)
    }
    
    // Readers
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 200; j++ {
                key := fmt.Sprintf("%d:10.%d.%d.0/24", id%5, id%5, j%100)
                table.Get(key)
            }
        }(i)
    }
    
    wg.Wait()
    
    stats := table.Stats()
    log.Printf("Stats: %+v", stats)
    log.Printf("Total routes: %d", len(table.GetAll()))
    
    // Batch processing demo
    processor := &BatchProcessor{table: table, workerCount: 4}
    routes := make([]Route, 50)
    for i := range routes {
        routes[i] = Route{
            VrfID:   uint32(i % 3),
            Prefix:  fmt.Sprintf("172.16.%d.0/24", i),
            NextHop: "10.0.0.1",
        }
    }
    
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    
    if err := processor.ProcessRoutes(ctx, routes); err != nil {
        log.Printf("Batch processing error: %v", err)
    }
    
    log.Printf("Final stats: %+v", table.Stats())
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 10. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYNC PRIMITIVES CHEATSHEET                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   sync.Mutex:                                                           │
│   • Always defer Unlock()                                               │
│   • Never copy                                                          │
│   • Document what it protects                                           │
│   • Lock ordering for multiple mutexes                                  │
│                                                                         │
│   sync.RWMutex:                                                         │
│   • Only when reads >> writes                                           │
│   • Profile before switching from Mutex                                 │
│                                                                         │
│   sync.WaitGroup:                                                       │
│   • Add() before goroutine starts                                       │
│   • Never Add() after Wait() might run                                  │
│   • Always Done() in defer                                              │
│                                                                         │
│   sync.Once:                                                            │
│   • Perfect for lazy initialization                                     │
│   • Panic in Do() still counts as "done"                               │
│                                                                         │
│   sync.Pool:                                                            │
│   • Profile before using                                                │
│   • Reset objects before returning                                      │
│   • No guarantee of reuse                                               │
│                                                                         │
│   sync/atomic:                                                          │
│   • Simple counters and flags                                           │
│   • Use mutex for related fields                                        │
│   • Prefer atomic types (Go 1.19+)                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### sync 包概览

| 类型 | 用途 | 使用场景 |
|------|------|----------|
| `sync.Mutex` | 互斥锁 | 保护共享状态 |
| `sync.RWMutex` | 读写锁 | 读多写少的场景 |
| `sync.WaitGroup` | 等待组 | 等待多个 goroutine 完成 |
| `sync.Once` | 一次性执行 | 单例、延迟初始化 |
| `sync.Pool` | 对象池 | 复用临时对象，减少分配 |
| `sync/atomic` | 原子操作 | 简单计数器、标志位 |

### 关键规则

1. **Mutex**
   - 总是用 `defer mu.Unlock()`
   - 永远不要复制 Mutex
   - 注释说明它保护什么
   - 多个锁时保持一致的加锁顺序

2. **RWMutex**
   - 只有在读远多于写时使用
   - 先 profile，再决定是否从 Mutex 切换

3. **WaitGroup**
   - `Add()` 必须在 goroutine 启动**之前**调用
   - `Done()` 放在 defer 中

4. **Once**
   - 适合延迟初始化
   - Do 中的 panic 也算"完成"

5. **Pool**
   - 先 profile 确认分配是瓶颈
   - 放回池之前必须重置对象状态

### Channel vs Sync 选择

**使用 Channel：**
- 传递数据所有权
- 协调 goroutine
- 分发工作
- 信号事件

**使用 Sync：**
- 保护共享状态（缓存、计数器、map）
- 简单的互斥
- 等待多个 goroutine
- 一次性初始化

