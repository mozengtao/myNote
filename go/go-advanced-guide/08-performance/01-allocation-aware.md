# Allocation-Aware Programming

## 1. Engineering Problem

### What real-world problem does this solve?

**Reducing allocations improves latency by decreasing GC pressure.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ALLOCATION IMPACT                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Every heap allocation:                                                │
│   ──────────────────────                                                │
│   • Takes time to allocate                                              │
│   • Adds to GC tracking overhead                                        │
│   • Eventually requires GC to clean up                                  │
│   • May cause GC pause                                                  │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Hot path optimization:                                                │
│                                                                         │
│   func handleRequest(r *Request) Response {                             │
│       // Called 10,000 times/second                                     │
│       buffer := make([]byte, 1024)    // Allocates each call!          │
│       // ...                                                            │
│   }                                                                     │
│                                                                         │
│   vs                                                                    │
│                                                                         │
│   var bufPool = sync.Pool{...}                                          │
│   func handleRequest(r *Request) Response {                             │
│       buffer := bufPool.Get().([]byte)                                  │
│       defer bufPool.Put(buffer)                                         │
│       // No allocation per call                                         │
│   }                                                                     │
│                                                                         │
│   Result: 10,000 allocs/sec → 0 allocs/sec                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Where allocations happen

```go
// Allocates:
s := make([]byte, 100)         // Slice backing array
m := make(map[string]int)      // Map internal structure
p := new(Route)                // Pointer to struct
r := &Route{}                  // Same as new
ch := make(chan int)           // Channel

// Escapes (allocates):
func escape() *int {
    x := 42
    return &x  // x escapes to heap
}

// Does NOT allocate:
var x int                      // Stack (usually)
arr := [100]byte{}             // Stack (if small enough)
s := arr[:]                    // Slice header only (3 words)
```

### Profile first

```bash
# Run benchmarks with memory profiling
go test -bench=. -benchmem

# Output
BenchmarkX-8    1000000    1500 ns/op    256 B/op    5 allocs/op

# Memory profile
go test -bench=. -memprofile=mem.prof
go tool pprof mem.prof
```

---

## 3. Language Mechanism

### sync.Pool for reusable objects

```go
var routePool = sync.Pool{
    New: func() interface{} {
        return &Route{}
    },
}

func processRoute() {
    r := routePool.Get().(*Route)
    defer routePool.Put(r)
    
    // Reset before use
    *r = Route{}
    
    // Use r...
}
```

### Pre-allocate slices

```go
// BAD: Multiple allocations as slice grows
func collect() []Route {
    var routes []Route
    for i := 0; i < n; i++ {
        routes = append(routes, getRoute(i))  // May reallocate
    }
    return routes
}

// GOOD: Single allocation
func collect() []Route {
    routes := make([]Route, 0, n)  // Pre-allocate capacity
    for i := 0; i < n; i++ {
        routes = append(routes, getRoute(i))  // No reallocation
    }
    return routes
}
```

### Reuse buffers

```go
// BAD: New buffer each call
func format(r Route) string {
    return fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)  // Allocates
}

// GOOD: Reuse buffer
func format(buf *bytes.Buffer, r Route) {
    buf.Reset()
    buf.WriteString(strconv.FormatUint(uint64(r.VrfID), 10))
    buf.WriteByte(':')
    buf.WriteString(r.Prefix)
}
```

### Avoid boxing (interface{})

```go
// BAD: Boxing allocates
func log(v interface{}) {
    fmt.Println(v)  // v is boxed
}

// Called in hot path
for _, r := range routes {
    log(r.VrfID)  // Allocates each time
}

// GOOD: Use specific types or generics
func logUint32(v uint32) {
    fmt.Println(v)  // No boxing
}
```

---

## 4. Idiomatic Usage

### Pattern 1: Buffer pooling

```go
var bufferPool = sync.Pool{
    New: func() interface{} {
        return bytes.NewBuffer(make([]byte, 0, 4096))
    },
}

func process(data []byte) []byte {
    buf := bufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        bufferPool.Put(buf)
    }()
    
    // Use buf...
    buf.Write(data)
    
    // Return copy if needed
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result
}
```

### Pattern 2: Slice reuse

```go
type RouteProcessor struct {
    scratch []Route  // Reusable scratch space
}

func (p *RouteProcessor) Process(routes []Route) []Route {
    // Reuse scratch, expand if needed
    if cap(p.scratch) < len(routes) {
        p.scratch = make([]Route, 0, len(routes))
    }
    p.scratch = p.scratch[:0]  // Reset length
    
    for _, r := range routes {
        if r.IsValid() {
            p.scratch = append(p.scratch, r)
        }
    }
    
    return p.scratch
}
```

### Pattern 3: Struct embedding to avoid pointer

```go
// More allocations: slice of pointers
type RouteManagerBad struct {
    routes []*Route  // Each Route is separate allocation
}

// Fewer allocations: slice of values
type RouteManagerGood struct {
    routes []Route  // Single contiguous allocation
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Premature optimization

```go
// DON'T optimize without measuring
// Profile first to find actual hot spots

// Run benchmark
go test -bench=BenchmarkHotPath -benchmem -cpuprofile=cpu.prof

// Analyze
go tool pprof cpu.prof
(pprof) top10
(pprof) list HotFunction
```

### Pitfall 2: Pool for cheap objects

```go
// BAD: Pool overhead > allocation cost
var intPool = sync.Pool{
    New: func() interface{} { return new(int) },
}

// Pool is for expensive objects (buffers, connections)
// Not for trivial types
```

### Pitfall 3: Returning pooled slice

```go
// BAD: Caller might hold reference after Put
func process() []byte {
    buf := bufferPool.Get().(*bytes.Buffer)
    buf.Write(data)
    result := buf.Bytes()  // Shares buffer!
    bufferPool.Put(buf)    // Now result is invalid!
    return result
}

// GOOD: Copy before Put
func process() []byte {
    buf := bufferPool.Get().(*bytes.Buffer)
    buf.Write(data)
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    bufferPool.Put(buf)
    return result
}
```

---

## 6. Complete Example

```go
package main

import (
    "bytes"
    "fmt"
    "runtime"
    "sync"
    "testing"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// Buffer pool for string formatting
var bufferPool = sync.Pool{
    New: func() interface{} {
        return bytes.NewBuffer(make([]byte, 0, 256))
    },
}

// BAD: Allocates on every call
func formatRouteBad(r Route) string {
    return fmt.Sprintf("%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
}

// GOOD: Reuses buffer
func formatRouteGood(r Route) string {
    buf := bufferPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        bufferPool.Put(buf)
    }()
    
    fmt.Fprintf(buf, "%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
    return buf.String()  // This still allocates, but buf is reused
}

// BEST: Caller provides buffer
func formatRouteBuffer(buf *bytes.Buffer, r Route) {
    buf.Reset()
    fmt.Fprintf(buf, "%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
}

// Pre-allocation example
func collectRoutesBad(n int) []Route {
    var routes []Route
    for i := 0; i < n; i++ {
        routes = append(routes, Route{VrfID: uint32(i)})
    }
    return routes
}

func collectRoutesGood(n int) []Route {
    routes := make([]Route, 0, n)  // Pre-allocate
    for i := 0; i < n; i++ {
        routes = append(routes, Route{VrfID: uint32(i)})
    }
    return routes
}

// Benchmarks
func BenchmarkFormatBad(b *testing.B) {
    r := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _ = formatRouteBad(r)
    }
}

func BenchmarkFormatGood(b *testing.B) {
    r := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _ = formatRouteGood(r)
    }
}

func BenchmarkFormatBuffer(b *testing.B) {
    r := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    buf := bytes.NewBuffer(make([]byte, 0, 256))
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        formatRouteBuffer(buf, r)
    }
}

func BenchmarkCollectBad(b *testing.B) {
    for i := 0; i < b.N; i++ {
        _ = collectRoutesBad(1000)
    }
}

func BenchmarkCollectGood(b *testing.B) {
    for i := 0; i < b.N; i++ {
        _ = collectRoutesGood(1000)
    }
}

func main() {
    var m runtime.MemStats
    
    // Before
    runtime.ReadMemStats(&m)
    before := m.TotalAlloc
    
    // Work
    for i := 0; i < 10000; i++ {
        _ = formatRouteGood(Route{VrfID: 1, Prefix: "10.0.0.0/24"})
    }
    
    // After
    runtime.ReadMemStats(&m)
    after := m.TotalAlloc
    
    fmt.Printf("Allocated: %d KB\n", (after-before)/1024)
}
```

Run benchmarks:
```bash
go test -bench=. -benchmem
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ALLOCATION RULES                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PROFILE FIRST                                                      │
│      • go test -bench=. -benchmem                                       │
│      • go tool pprof                                                    │
│      • Don't guess - measure                                            │
│                                                                         │
│   2. PRE-ALLOCATE SLICES                                                │
│      • make([]T, 0, capacity)                                           │
│      • When size is known or estimable                                  │
│                                                                         │
│   3. USE sync.Pool FOR EXPENSIVE OBJECTS                                │
│      • Buffers, connections                                             │
│      • Not for trivial types                                            │
│      • Reset before Put                                                 │
│                                                                         │
│   4. AVOID ESCAPE TO HEAP                                               │
│      • Return values, not pointers                                      │
│      • Avoid interface{} in hot paths                                   │
│      • go build -gcflags="-m" to check                                  │
│                                                                         │
│   5. REUSE BUFFERS                                                      │
│      • Pass buffer to functions                                         │
│      • bytes.Buffer.Reset()                                             │
│                                                                         │
│   6. []T OVER []*T                                                      │
│      • Fewer allocations                                                │
│      • Better cache locality                                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 分配成本

每次堆分配：
- 消耗分配时间
- 增加 GC 追踪开销
- 最终需要 GC 清理
- 可能导致 GC 暂停

### 优化技巧

| 技巧 | 说明 |
|------|------|
| 预分配切片 | `make([]T, 0, n)` |
| sync.Pool | 重用昂贵对象 |
| 避免逃逸 | 返回值而非指针 |
| 重用缓冲区 | 传入 buffer |
| []T 而非 []*T | 减少分配 |

### 基准测试

```bash
go test -bench=. -benchmem

# 输出
BenchmarkX  1000000  1500 ns/op  256 B/op  5 allocs/op
#           执行次数  每次耗时     每次内存   每次分配次数
```

### 最佳实践

1. **先测量后优化**
2. **预分配已知大小**
3. **sync.Pool 用于昂贵对象**
4. **避免 interface{} 装箱**
5. **传入缓冲区而非返回**

