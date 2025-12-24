# Object Lifetime: References and Cleanup

## 1. Engineering Problem

### What real-world problem does this solve?

**Understanding object lifetime prevents memory leaks and ensures proper resource cleanup.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    OBJECT LIFETIME IN GO                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Object lifecycle:                                                     │
│   ──────────────────                                                    │
│                                                                         │
│   ┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐     │
│   │  Created  │───►│   Used    │───►│ Unreached │───►│ Collected │     │
│   └───────────┘    └───────────┘    └───────────┘    └───────────┘     │
│        │                │                │                │             │
│        ▼                ▼                ▼                ▼             │
│   Allocated on     Referenced by    No more refs     GC reclaims       │
│   stack or heap    live code        (eligible)       memory            │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   What keeps objects alive:                                             │
│                                                                         │
│   • Global variables                                                    │
│   • Stack variables in active functions                                 │
│   • Pointers from other live objects                                    │
│   • Goroutine stacks                                                    │
│   • Closed-over variables in running goroutines                         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Common lifetime issues:                                               │
│                                                                         │
│   • Goroutine holding reference → object lives too long                │
│   • Closure capturing variable → unexpected retention                   │
│   • Slice header retaining backing array                                │
│   • Cache without eviction → unbounded growth                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Reference chains

```go
type Server struct {
    cache *Cache     // Server keeps Cache alive
}

type Cache struct {
    data map[string]*Entry  // Cache keeps all Entries alive
}

type Entry struct {
    Value []byte     // Entry keeps byte slice alive
}

// If server is reachable, entire chain is alive
// Even if Entry is "logically deleted" from map,
// if any reference to it exists, it can't be GC'd
```

### Closure capture

```go
func process(data []byte) {
    // Closure captures 'data'
    go func() {
        time.Sleep(time.Hour)
        use(data)  // data lives at least 1 hour!
    }()
}

// Even after process() returns, data can't be GC'd
// because goroutine holds reference
```

---

## 3. Language Mechanism

### Finalizers

```go
import "runtime"

type Resource struct {
    handle uintptr
}

func NewResource() *Resource {
    r := &Resource{handle: acquireHandle()}
    
    // Set finalizer - called before GC collects object
    runtime.SetFinalizer(r, func(r *Resource) {
        releaseHandle(r.handle)
    })
    
    return r
}

// WARNING: Finalizers are unreliable!
// - Not guaranteed to run
// - No defined order
// - May delay GC
// Prefer explicit Close() methods
```

### Weak references (Go 1.23+)

```go
import "weak"

type Cache struct {
    entries map[string]weak.Pointer[*Entry]
}

// Weak pointer doesn't prevent GC
// Entry can be collected even if in cache
func (c *Cache) Get(key string) *Entry {
    if wp, ok := c.entries[key]; ok {
        return wp.Value()  // nil if collected
    }
    return nil
}
```

### Breaking reference cycles

```go
// Potential issue: circular reference
type Parent struct {
    child *Child
}

type Child struct {
    parent *Parent  // Creates cycle
}

// In Go, GC handles cycles - no leak
// But consider if you need bidirectional refs

// Often better: pass parent as parameter
type Child struct {
    // No parent field
}

func (c *Child) DoWork(p *Parent) {
    // Use parent only when needed
}
```

---

## 4. Idiomatic Usage

### Explicit cleanup

```go
type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]*Route
    done   chan struct{}
}

func NewRouteManager() *RouteManager {
    rm := &RouteManager{
        routes: make(map[string]*Route),
        done:   make(chan struct{}),
    }
    go rm.cleanup()
    return rm
}

// Explicit cleanup method
func (rm *RouteManager) Close() {
    close(rm.done)  // Signal cleanup goroutine to stop
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    // Clear references
    for k := range rm.routes {
        delete(rm.routes, k)
    }
    rm.routes = nil
}

func (rm *RouteManager) cleanup() {
    ticker := time.NewTicker(time.Minute)
    defer ticker.Stop()
    
    for {
        select {
        case <-rm.done:
            return
        case <-ticker.C:
            rm.evictExpired()
        }
    }
}
```

### Cache with TTL

```go
type CachedRoute struct {
    Route     Route
    ExpiresAt time.Time
}

type RouteCache struct {
    mu    sync.RWMutex
    items map[string]*CachedRoute
}

func (c *RouteCache) Set(key string, r Route, ttl time.Duration) {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    c.items[key] = &CachedRoute{
        Route:     r,
        ExpiresAt: time.Now().Add(ttl),
    }
}

func (c *RouteCache) Get(key string) (Route, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    
    item, ok := c.items[key]
    if !ok || time.Now().After(item.ExpiresAt) {
        return Route{}, false
    }
    return item.Route, true
}

func (c *RouteCache) Evict() {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    now := time.Now()
    for k, v := range c.items {
        if now.After(v.ExpiresAt) {
            delete(c.items, k)
        }
    }
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Goroutine keeps reference alive

```go
// BAD: largeData lives until goroutine completes
func process(largeData []byte) {
    go func() {
        time.Sleep(time.Hour)
        checksum := md5.Sum(largeData)
        log.Println(checksum)
    }()
}

// GOOD: Process immediately, don't hold reference
func process(largeData []byte) {
    checksum := md5.Sum(largeData)  // Process now
    go func() {
        time.Sleep(time.Hour)
        log.Println(checksum)  // Only hold small result
    }()
}
```

### Pitfall 2: Slice retains backing array

```go
// BAD: small slice keeps large array alive
func getPrefix(large []byte) []byte {
    return large[:10]  // Entire backing array retained!
}

// GOOD: Copy to break reference
func getPrefix(large []byte) []byte {
    result := make([]byte, 10)
    copy(result, large[:10])
    return result
}
```

### Pitfall 3: Unbounded cache

```go
// BAD: Cache grows forever
var cache = make(map[string]Result)

func Get(key string) Result {
    if r, ok := cache[key]; ok {
        return r
    }
    r := compute(key)
    cache[key] = r  // Never evicted!
    return r
}

// GOOD: Use LRU or TTL cache
var cache = lru.New(1000)  // Max 1000 entries
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

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
    data    []byte  // Large payload
}

// RouteCache with TTL and size limit
type RouteCache struct {
    mu       sync.RWMutex
    items    map[string]*cachedItem
    maxSize  int
    done     chan struct{}
}

type cachedItem struct {
    route     *Route
    expiresAt time.Time
    accessedAt time.Time
}

func NewRouteCache(maxSize int) *RouteCache {
    c := &RouteCache{
        items:   make(map[string]*cachedItem),
        maxSize: maxSize,
        done:    make(chan struct{}),
    }
    go c.evictionLoop()
    return c
}

func (c *RouteCache) Set(key string, r *Route, ttl time.Duration) {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    // Evict if at capacity
    if len(c.items) >= c.maxSize {
        c.evictOldest()
    }
    
    c.items[key] = &cachedItem{
        route:      r,
        expiresAt:  time.Now().Add(ttl),
        accessedAt: time.Now(),
    }
}

func (c *RouteCache) Get(key string) (*Route, bool) {
    c.mu.RLock()
    item, ok := c.items[key]
    c.mu.RUnlock()
    
    if !ok || time.Now().After(item.expiresAt) {
        return nil, false
    }
    
    // Update access time
    c.mu.Lock()
    if item, ok := c.items[key]; ok {
        item.accessedAt = time.Now()
    }
    c.mu.Unlock()
    
    return item.route, true
}

func (c *RouteCache) evictOldest() {
    var oldestKey string
    var oldestTime time.Time
    
    for k, v := range c.items {
        if oldestKey == "" || v.accessedAt.Before(oldestTime) {
            oldestKey = k
            oldestTime = v.accessedAt
        }
    }
    
    if oldestKey != "" {
        delete(c.items, oldestKey)
    }
}

func (c *RouteCache) evictionLoop() {
    ticker := time.NewTicker(time.Minute)
    defer ticker.Stop()
    
    for {
        select {
        case <-c.done:
            return
        case <-ticker.C:
            c.evictExpired()
        }
    }
}

func (c *RouteCache) evictExpired() {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    now := time.Now()
    for k, v := range c.items {
        if now.After(v.expiresAt) {
            delete(c.items, k)
        }
    }
}

func (c *RouteCache) Close() {
    close(c.done)
    
    c.mu.Lock()
    defer c.mu.Unlock()
    
    // Clear all references
    for k := range c.items {
        delete(c.items, k)
    }
    c.items = nil
}

func (c *RouteCache) Stats() (size int, memMB uint64) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    
    return len(c.items), m.HeapAlloc / 1024 / 1024
}

func main() {
    cache := NewRouteCache(100)
    defer cache.Close()
    
    // Add routes with TTL
    for i := 0; i < 200; i++ {
        route := &Route{
            VrfID:   uint32(i % 10),
            Prefix:  fmt.Sprintf("10.%d.%d.0/24", i/256, i%256),
            NextHop: "192.168.1.1",
            data:    make([]byte, 1024),  // 1KB payload
        }
        cache.Set(route.Prefix, route, 5*time.Minute)
    }
    
    size, mem := cache.Stats()
    fmt.Printf("Cache: %d items, %d MB heap\n", size, mem)
    
    // Access some items
    for i := 0; i < 50; i++ {
        key := fmt.Sprintf("10.%d.%d.0/24", i/256, i%256)
        if r, ok := cache.Get(key); ok {
            fmt.Printf("Hit: %s\n", r.Prefix)
        }
    }
    
    runtime.GC()
    size, mem = cache.Stats()
    fmt.Printf("After GC: %d items, %d MB heap\n", size, mem)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    OBJECT LIFETIME RULES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PREFER EXPLICIT CLEANUP OVER FINALIZERS                            │
│      • Implement Close() method                                         │
│      • Use defer to ensure cleanup                                      │
│      • Finalizers are unreliable                                        │
│                                                                         │
│   2. BE AWARE OF REFERENCE RETENTION                                    │
│      • Goroutines keep captured variables alive                         │
│      • Slice headers retain backing arrays                              │
│      • Map entries are never shrunk                                     │
│                                                                         │
│   3. BOUND CACHE SIZE                                                   │
│      • Use LRU eviction                                                 │
│      • Use TTL expiration                                               │
│      • Never unbounded growth                                           │
│                                                                         │
│   4. BREAK REFERENCES WHEN DONE                                         │
│      • Set to nil if keeping container                                  │
│      • Copy small slices from large arrays                              │
│      • Clear maps instead of deleting entries                           │
│                                                                         │
│   5. SIGNAL GOROUTINES TO STOP                                          │
│      • Use done channel or context                                      │
│      • Goroutines hold references until stopped                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 对象生命周期

```
创建 → 使用 → 不可达 → 回收
```

### 保持对象存活的引用

- 全局变量
- 活动函数的栈变量
- 其他存活对象的指针
- Goroutine 栈
- 闭包捕获的变量

### 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| Goroutine 保持引用 | 闭包捕获 | 提前处理数据 |
| 切片保持数组 | 共享底层数组 | 复制需要的部分 |
| 无界缓存 | 只增不减 | 使用 LRU/TTL |

### 最佳实践

1. **显式清理优于 Finalizer**
2. **注意引用保留**
3. **限制缓存大小**
4. **完成后断开引用**
5. **通知 Goroutine 停止**

