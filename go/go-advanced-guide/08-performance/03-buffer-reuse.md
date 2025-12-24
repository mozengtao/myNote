# Buffer Reuse: sync.Pool

## 1. Engineering Problem

### What real-world problem does this solve?

**High-throughput services create garbage faster than GC can collect, causing latency spikes.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GC PRESSURE PROBLEM                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without pooling:                                                      │
│   ────────────────                                                      │
│                                                                         │
│   Request 1: alloc buffer → use → discard                              │
│   Request 2: alloc buffer → use → discard     ──► GC runs             │
│   Request 3: alloc buffer → use → discard         (latency spike!)     │
│   ...                                                                   │
│   Request 10000: GC can't keep up! Memory grows.                       │
│                                                                         │
│   With sync.Pool:                                                       │
│   ───────────────                                                       │
│                                                                         │
│   Request 1: get from pool → use → return to pool                      │
│   Request 2: get from pool → use → return to pool                      │
│   Request 3: get from pool → use → return to pool                      │
│   ...                                                                   │
│   Same buffers recycled, fewer allocations, less GC pressure           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Pool items used after Put (data corruption)
- Forgetting to Reset before Put (memory leak in pool)
- Using pool for small/cheap objects (overhead > benefit)
- Storing pointers to pooled objects (dangling references)

---

## 2. Core Mental Model

### How Go expects you to think

**sync.Pool is a cache of temporary objects that may be cleared at any GC.**

```go
// Pool provides Get and Put
pool.Get()  → Retrieve object (or create via New)
pool.Put()  → Return object to pool (may be GC'd later)
```

### Key properties

1. **Thread-safe**: Multiple goroutines can Get/Put concurrently
2. **Per-P storage**: Each processor has local pool (reduces contention)
3. **GC clears pools**: Objects may be collected between GC cycles
4. **No guarantees**: Get may return nil or create new

### Philosophy

- Reduce allocation rate, not total allocations
- Best for short-lived, frequently allocated objects
- Not a cache - objects may disappear

---

## 3. Language Mechanism

### Basic pool

```go
var bufferPool = sync.Pool{
    New: func() interface{} {
        return new(bytes.Buffer)
    },
}

func useBuffer() {
    // Get buffer
    buf := bufferPool.Get().(*bytes.Buffer)
    
    // Use it
    buf.WriteString("data")
    
    // Reset and return
    buf.Reset()
    bufferPool.Put(buf)
}
```

### Pool without New function

```go
var pool = sync.Pool{}

func get() *Buffer {
    if v := pool.Get(); v != nil {
        return v.(*Buffer)
    }
    return &Buffer{}  // Create if pool empty
}
```

### Generic pool (Go 1.18+)

```go
type Pool[T any] struct {
    p sync.Pool
}

func NewPool[T any](new func() T) *Pool[T] {
    return &Pool[T]{
        p: sync.Pool{New: func() interface{} { return new() }},
    }
}

func (p *Pool[T]) Get() T {
    return p.p.Get().(T)
}

func (p *Pool[T]) Put(v T) {
    p.p.Put(v)
}
```

---

## 4. Idiomatic Usage

### When to use

- Byte buffers for serialization
- Temporary slices in hot paths
- Connection wrappers
- Encoder/decoder state

### When NOT to use

- Small objects (< 256 bytes)
- Long-lived objects
- Objects with expensive initialization
- Objects rarely allocated

### Pattern: Buffer pool

```go
var bufPool = sync.Pool{
    New: func() interface{} {
        return bytes.NewBuffer(make([]byte, 0, 4096))
    },
}

func format(r Route) []byte {
    buf := bufPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        bufPool.Put(buf)
    }()
    
    fmt.Fprintf(buf, "%d:%s", r.VrfID, r.Prefix)
    
    // Copy result - don't return pooled buffer!
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result
}
```

### Pattern: Slice pool

```go
var slicePool = sync.Pool{
    New: func() interface{} {
        s := make([]byte, 0, 64*1024)  // 64KB
        return &s
    },
}

func processData(data []byte) {
    sp := slicePool.Get().(*[]byte)
    defer slicePool.Put(sp)
    
    buf := (*sp)[:0]  // Reset length
    buf = append(buf, data...)
    // Process buf...
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Using object after Put

```go
// BAD: Use after Put
buf := bufPool.Get().(*bytes.Buffer)
bufPool.Put(buf)
buf.WriteString("data")  // DANGER: Another goroutine may have it!

// GOOD: Use defer, or ensure Put is last
buf := bufPool.Get().(*bytes.Buffer)
defer func() {
    buf.Reset()
    bufPool.Put(buf)
}()
buf.WriteString("data")  // Safe
```

### Pitfall 2: Returning pooled buffer directly

```go
// BAD: Caller holds reference to pooled object
func format(r Route) *bytes.Buffer {
    buf := bufPool.Get().(*bytes.Buffer)
    fmt.Fprintf(buf, "%d:%s", r.VrfID, r.Prefix)
    bufPool.Put(buf)  // Oops! Still returning buf
    return buf        // Caller gets corrupted data!
}

// GOOD: Copy data out
func format(r Route) []byte {
    buf := bufPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        bufPool.Put(buf)
    }()
    
    fmt.Fprintf(buf, "%d:%s", r.VrfID, r.Prefix)
    
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result
}
```

### Pitfall 3: Forgetting to Reset

```go
// BAD: Buffer accumulates data
func process(data []byte) {
    buf := bufPool.Get().(*bytes.Buffer)
    buf.Write(data)
    // ... use buf ...
    bufPool.Put(buf)  // Still has data!
}

// Next Get returns buffer with old data!

// GOOD: Reset before Put
func process(data []byte) {
    buf := bufPool.Get().(*bytes.Buffer)
    buf.Write(data)
    // ... use buf ...
    buf.Reset()       // Clear data
    bufPool.Put(buf)
}
```

### Pitfall 4: Pooling cheap objects

```go
// BAD: Overhead > benefit for small objects
var intPool = sync.Pool{
    New: func() interface{} { return new(int) },
}

// GOOD: Just allocate
func useInt() {
    n := new(int)  // Cheap, stack-allocated anyway
    *n = 42
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "net/http"
    "sync"
)

type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

// Buffer pool for JSON encoding
var jsonBufferPool = sync.Pool{
    New: func() interface{} {
        return bytes.NewBuffer(make([]byte, 0, 1024))
    },
}

// Encoder pool
var encoderPool = sync.Pool{
    New: func() interface{} {
        return json.NewEncoder(nil)
    },
}

// RouteFormatter uses pooled buffers
type RouteFormatter struct{}

func (f *RouteFormatter) Format(route Route) ([]byte, error) {
    // Get buffer from pool
    buf := jsonBufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        jsonBufferPool.Put(buf)
    }()
    
    // Encode route
    if err := json.NewEncoder(buf).Encode(route); err != nil {
        return nil, err
    }
    
    // Copy result (don't return pooled buffer!)
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result, nil
}

func (f *RouteFormatter) FormatMany(routes []Route) ([]byte, error) {
    buf := jsonBufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        jsonBufferPool.Put(buf)
    }()
    
    buf.WriteByte('[')
    for i, r := range routes {
        if i > 0 {
            buf.WriteByte(',')
        }
        data, err := json.Marshal(r)
        if err != nil {
            return nil, err
        }
        buf.Write(data)
    }
    buf.WriteByte(']')
    
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result, nil
}

// HTTP handler using buffer pool
type RouteHandler struct {
    routes []Route
    mu     sync.RWMutex
}

func (h *RouteHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    h.mu.RLock()
    routes := h.routes
    h.mu.RUnlock()
    
    // Get buffer from pool
    buf := jsonBufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        jsonBufferPool.Put(buf)
    }()
    
    // Encode to pooled buffer
    encoder := json.NewEncoder(buf)
    if err := encoder.Encode(routes); err != nil {
        http.Error(w, err.Error(), 500)
        return
    }
    
    // Write response
    w.Header().Set("Content-Type", "application/json")
    w.Header().Set("Content-Length", fmt.Sprintf("%d", buf.Len()))
    w.Write(buf.Bytes())  // Direct write, no copy needed
}

// Benchmark comparison
func BenchmarkWithoutPool(routes []Route) []byte {
    buf := new(bytes.Buffer)
    json.NewEncoder(buf).Encode(routes)
    return buf.Bytes()
}

func BenchmarkWithPool(routes []Route) []byte {
    buf := jsonBufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        jsonBufferPool.Put(buf)
    }()
    
    json.NewEncoder(buf).Encode(routes)
    
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result
}

func main() {
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
    }
    
    formatter := &RouteFormatter{}
    
    // Format single route
    data, err := formatter.Format(routes[0])
    if err != nil {
        panic(err)
    }
    fmt.Printf("Single: %s\n", data)
    
    // Format multiple routes
    data, err = formatter.FormatMany(routes)
    if err != nil {
        panic(err)
    }
    fmt.Printf("Many: %s\n", data)
    
    // HTTP server with pooled buffers
    handler := &RouteHandler{routes: routes}
    http.Handle("/routes", handler)
    fmt.Println("Server on :8080")
    // http.ListenAndServe(":8080", nil)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYNC.POOL RULES                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. RESET BEFORE PUT                                                   │
│      • Clear buffer contents                                            │
│      • Reset slice length                                               │
│      • Prevents memory leak in pool                                     │
│                                                                         │
│   2. COPY DATA OUT                                                      │
│      • Never return pooled object                                       │
│      • Copy bytes before Put                                            │
│      • Caller can't hold reference                                      │
│                                                                         │
│   3. DON'T USE AFTER PUT                                                │
│      • Put should be last operation                                     │
│      • Use defer for safety                                             │
│                                                                         │
│   4. POOL EXPENSIVE OBJECTS                                             │
│      • Large buffers (>1KB)                                             │
│      • Objects with expensive init                                      │
│      • NOT for small/cheap objects                                      │
│                                                                         │
│   5. BENCHMARK TO VERIFY BENEFIT                                        │
│      • Pool adds overhead                                               │
│      • Not always faster                                                │
│      • Measure allocation reduction                                     │
│                                                                         │
│   6. POOL IS NOT A CACHE                                                │
│      • Objects may be GC'd                                              │
│      • No size limit                                                    │
│      • No LRU eviction                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### sync.Pool 核心概念

**sync.Pool 是临时对象的缓存，可能在任何 GC 时被清除。**

### 基本用法

```go
var pool = sync.Pool{
    New: func() interface{} {
        return new(bytes.Buffer)
    },
}

buf := pool.Get().(*bytes.Buffer)
defer func() {
    buf.Reset()
    pool.Put(buf)
}()
```

### 常见陷阱

| 陷阱 | 问题 | 解决方案 |
|------|------|----------|
| Put 后使用 | 数据损坏 | 用 defer |
| 返回池化对象 | 悬空引用 | 复制数据 |
| 忘记 Reset | 内存泄漏 | Put 前 Reset |
| 池化小对象 | 开销大于收益 | 只池化大对象 |

### 最佳实践

1. Put 前 Reset
2. 复制数据再 Put
3. 用 defer 确保归还
4. 只池化大/昂贵对象
5. 基准测试验证收益
