# Garbage Collection in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**Go's GC trades some performance for safety and simplicity.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MEMORY MANAGEMENT SPECTRUM                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Manual (C/C++)              GC (Go)                 Full GC (Java)    │
│   ─────────────────           ────────                ──────────────    │
│                                                                         │
│   malloc/free                 Concurrent,             Stop-the-world,   │
│   Full control                low-latency GC          generational,     │
│   Max performance                                     optimized for     │
│   Memory bugs risk                                    throughput        │
│                                                                         │
│   ┌────────────────────────────────────────────────────────────────┐    │
│   │ Control                                      Convenience        │    │
│   │ ◄────────────────────────────────────────────────────────────► │    │
│   │        C/C++         Go              Java                      │    │
│   └────────────────────────────────────────────────────────────────┘    │
│                                                                         │
│   Go's design goals:                                                    │
│   • Sub-millisecond pause times                                         │
│   • Concurrent with application                                         │
│   • Simple tuning (just GOGC)                                           │
│   • Good enough for most server workloads                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### How Go's GC works

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GO GC OVERVIEW                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Go uses a CONCURRENT, TRI-COLOR, MARK-AND-SWEEP collector            │
│                                                                         │
│   Tri-color marking:                                                    │
│   ──────────────────                                                    │
│                                                                         │
│   WHITE: Not yet seen (candidates for collection)                       │
│   GREY:  Seen, but children not yet scanned                            │
│   BLACK: Scanned completely (will be kept)                              │
│                                                                         │
│   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐             │
│   │    WHITE     │───►│    GREY      │───►│    BLACK     │             │
│   │  (unknown)   │    │  (pending)   │    │  (reachable) │             │
│   └──────────────┘    └──────────────┘    └──────────────┘             │
│                                                                         │
│   At end: WHITE objects are garbage, BLACK are kept                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   GC phases:                                                            │
│                                                                         │
│   1. MARK START (STW ~10-30μs)                                          │
│      • Enable write barrier                                             │
│      • Start marking from roots                                         │
│                                                                         │
│   2. MARKING (concurrent with app)                                      │
│      • Traverse object graph                                            │
│      • Application continues running                                    │
│                                                                         │
│   3. MARK TERMINATION (STW ~10-30μs)                                    │
│      • Finish marking                                                   │
│      • Disable write barrier                                            │
│                                                                         │
│   4. SWEEPING (concurrent)                                              │
│      • Reclaim white objects                                            │
│      • Prepare for next cycle                                           │
│                                                                         │
│   STW = Stop-The-World (brief pause)                                    │
│   Total STW typically < 1ms                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Triggering GC

```go
// GC triggers automatically when:
// 1. Heap grows by GOGC percent (default 100 = 2x)
// 2. Time-based (if no allocations for 2 minutes)

// Force GC (rarely needed)
runtime.GC()

// Get GC statistics
var m runtime.MemStats
runtime.ReadMemStats(&m)
fmt.Printf("Heap: %d MB\n", m.HeapAlloc/1024/1024)
fmt.Printf("GC cycles: %d\n", m.NumGC)
```

### GOGC environment variable

```bash
# Default: GC when heap doubles
GOGC=100 ./myapp

# More aggressive: GC when heap grows 50%
GOGC=50 ./myapp

# Less aggressive: GC when heap grows 200%
GOGC=200 ./myapp

# Disable GC (for benchmarking)
GOGC=off ./myapp
```

### GOMEMLIMIT (Go 1.19+)

```bash
# Set soft memory limit
GOMEMLIMIT=1GiB ./myapp

# Useful for containerized environments
# GC becomes more aggressive as memory approaches limit
```

---

## 4. Idiomatic Usage

### Reduce allocations

```go
// BAD: Allocates on every call
func process(items []string) []Result {
    var results []Result
    for _, item := range items {
        results = append(results, process(item))
    }
    return results
}

// GOOD: Pre-allocate
func process(items []string) []Result {
    results := make([]Result, 0, len(items))  // Pre-allocate capacity
    for _, item := range items {
        results = append(results, process(item))
    }
    return results
}
```

### Reuse buffers with sync.Pool

```go
var bufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}

func processRequest(data []byte) {
    buf := bufferPool.Get().([]byte)
    buf = buf[:0]  // Reset length
    
    // Use buffer...
    
    bufferPool.Put(buf)  // Return to pool
}
```

### Reduce pointer chasing

```go
// More pointers = more GC work
type Bad struct {
    items []*Item  // Slice of pointers
}

// Fewer pointers = less GC work
type Good struct {
    items []Item  // Slice of values (if Item is not too large)
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Allocation in hot path

```go
// BAD: Allocates string every call
func formatKey(vrf uint32, prefix string) string {
    return fmt.Sprintf("%d:%s", vrf, prefix)  // Allocates
}

// BETTER: Use string builder
func formatKey(buf *strings.Builder, vrf uint32, prefix string) {
    buf.Reset()
    buf.WriteString(strconv.FormatUint(uint64(vrf), 10))
    buf.WriteString(":")
    buf.WriteString(prefix)
}
```

### Pitfall 2: Large heap = longer GC pauses

```go
// 100GB heap with GOGC=100 means GC at 200GB
// Marking 100GB takes time

// Solutions:
// 1. GOMEMLIMIT to cap memory
// 2. Lower GOGC for more frequent, shorter GCs
// 3. Reduce live data size
```

### Pitfall 3: Reference retention

```go
// BUG: Slice header keeps entire underlying array
func getFirst(large []Route) Route {
    return large[:1][0]  // Still references full array!
}

// FIX: Copy to break reference
func getFirst(large []Route) Route {
    result := make([]Route, 1)
    copy(result, large[:1])
    return result[0]
}
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "runtime"
    "sync"
    "time"
)

// RouteCache with GC-friendly design
type RouteCache struct {
    mu       sync.RWMutex
    routes   []Route  // Value slice, not pointer slice
    routeMap map[string]int  // Map to index, not to *Route
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

func NewRouteCache(capacity int) *RouteCache {
    return &RouteCache{
        routes:   make([]Route, 0, capacity),  // Pre-allocate
        routeMap: make(map[string]int, capacity),
    }
}

func (c *RouteCache) Add(r Route) {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    if idx, ok := c.routeMap[key]; ok {
        c.routes[idx] = r
        return
    }
    
    c.routeMap[key] = len(c.routes)
    c.routes = append(c.routes, r)
}

func (c *RouteCache) Get(vrfID uint32, prefix string) (Route, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    if idx, ok := c.routeMap[key]; ok {
        return c.routes[idx], true
    }
    return Route{}, false
}

// Buffer pool for reducing allocations
var keyBufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 0, 64)
    },
}

func printGCStats() {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    
    fmt.Printf("HeapAlloc: %d MB\n", m.HeapAlloc/1024/1024)
    fmt.Printf("HeapObjects: %d\n", m.HeapObjects)
    fmt.Printf("NumGC: %d\n", m.NumGC)
    fmt.Printf("LastGC: %v ago\n", time.Since(time.Unix(0, int64(m.LastGC))))
}

func main() {
    cache := NewRouteCache(10000)
    
    // Add routes
    for i := 0; i < 10000; i++ {
        cache.Add(Route{
            VrfID:   uint32(i % 10),
            Prefix:  fmt.Sprintf("10.%d.%d.0/24", i/256, i%256),
            NextHop: "192.168.1.1",
        })
    }
    
    fmt.Println("After adding routes:")
    printGCStats()
    
    runtime.GC()
    
    fmt.Println("\nAfter explicit GC:")
    printGCStats()
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GC OPTIMIZATION RULES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. REDUCE ALLOCATIONS                                                 │
│      • Pre-allocate slices with known capacity                          │
│      • Use sync.Pool for frequently allocated objects                   │
│      • Avoid allocations in hot paths                                   │
│                                                                         │
│   2. REDUCE POINTERS                                                    │
│      • Prefer []T over []*T when possible                               │
│      • Use value types for small structs                                │
│      • Fewer pointers = less GC scanning                                │
│                                                                         │
│   3. CONTROL HEAP SIZE                                                  │
│      • Use GOMEMLIMIT in containers                                     │
│      • Tune GOGC based on latency vs memory trade-off                   │
│      • Don't hoard memory (return buffers to pools)                     │
│                                                                         │
│   4. PROFILE BEFORE OPTIMIZING                                          │
│      • Use pprof to identify allocation hot spots                       │
│      • Measure GC pause times                                           │
│      • Don't optimize without data                                      │
│                                                                         │
│   5. GC TUNING KNOBS:                                                   │
│      • GOGC: trade memory for CPU (higher = less GC)                    │
│      • GOMEMLIMIT: hard cap on memory usage                             │
│      • runtime.GC(): force GC (testing only)                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Go GC 特点

| 特性 | 说明 |
|------|------|
| 并发 | 与应用程序并发运行 |
| 三色标记 | 白色（未知）→ 灰色（待扫描）→ 黑色（可达） |
| 低延迟 | STW 暂停通常 < 1ms |
| 简单调优 | 主要用 GOGC |

### GC 阶段

1. **标记开始**（STW ~10-30μs）：启用写屏障
2. **标记**（并发）：遍历对象图
3. **标记终止**（STW ~10-30μs）：完成标记
4. **清扫**（并发）：回收白色对象

### 调优参数

| 参数 | 作用 |
|------|------|
| GOGC=100 | 默认：堆翻倍时触发 GC |
| GOGC=50 | 更激进：堆增长 50% 时触发 |
| GOMEMLIMIT | 软内存限制（Go 1.19+） |

### 优化原则

1. **减少分配**：预分配、使用 sync.Pool
2. **减少指针**：用 []T 而非 []*T
3. **控制堆大小**：使用 GOMEMLIMIT
4. **先测量后优化**：使用 pprof

