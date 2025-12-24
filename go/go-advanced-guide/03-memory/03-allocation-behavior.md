# Allocation Behavior: Cost and Patterns

## 1. Engineering Problem

### What real-world problem does this solve?

**Understanding allocation costs helps write efficient Go code without premature optimization.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ALLOCATION COST MODEL                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Stack Allocation:                  Heap Allocation:                   │
│   ─────────────────                  ─────────────────                  │
│   • ~1 instruction                   • ~100+ instructions               │
│   • No GC overhead                   • GC must track & scan             │
│   • Automatic cleanup                • GC must collect                  │
│   • Local to goroutine               • Shared across goroutines         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   What allocates on heap:                                               │
│                                                                         │
│   make([]T, n)     →  Backing array on heap                            │
│   make(map[K]V)    →  Map structure on heap                            │
│   make(chan T)     →  Channel on heap                                  │
│   new(T), &T{}     →  If T escapes (returned, captured, etc.)          │
│   append()         →  May reallocate if capacity exceeded               │
│   string concat    →  Creates new string                                │
│   interface{}      →  Boxing allocates                                  │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Cost comparison (approximate):                                        │
│                                                                         │
│   Stack alloc:     ~1 ns                                                │
│   Small heap:      ~25 ns                                               │
│   Large heap:      ~100+ ns                                             │
│   GC mark/scan:    ~1 ns per pointer                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Slice allocation

```go
// Initial allocation
s := make([]int, 0, 100)  // Allocates backing array for 100 ints

// Append within capacity - no allocation
for i := 0; i < 100; i++ {
    s = append(s, i)  // Uses existing capacity
}

// Append beyond capacity - REALLOCATES
s = append(s, 101)  // New backing array (usually 2x size)
```

### Map allocation

```go
// Initial allocation with hint
m := make(map[string]int, 100)  // Pre-sized for ~100 entries

// Adding entries
m["key"] = value  // May trigger internal reallocation if too full

// Map internals grow automatically but never shrink
```

### String allocation

```go
// String concatenation allocates
s := "hello" + " " + "world"  // 2 allocations (intermediate + final)

// Better: strings.Builder
var b strings.Builder
b.WriteString("hello")
b.WriteString(" ")
b.WriteString("world")
result := b.String()  // 1 allocation at end
```

---

## 3. Language Mechanism

### Viewing allocations

```bash
# Escape analysis
go build -gcflags="-m" ./...

# Benchmark with allocs
go test -bench=. -benchmem

# Output: BenchmarkX  1000  1500 ns/op  256 B/op  5 allocs/op
```

### Common allocation sources

```go
// Slice growth
s := []int{}
for i := 0; i < 1000; i++ {
    s = append(s, i)  // Multiple reallocations
}

// String building
var result string
for i := 0; i < 1000; i++ {
    result += fmt.Sprintf("%d", i)  // O(n²) allocations!
}

// Interface boxing
func log(v interface{}) { fmt.Println(v) }
log(42)  // int boxed to interface{}

// Closure capture
for i := 0; i < 10; i++ {
    go func() { use(i) }()  // i escapes to heap
}
```

---

## 4. Idiomatic Usage

### Pre-allocation

```go
// Know the size? Pre-allocate!
routes := make([]Route, 0, len(input))
for _, r := range input {
    routes = append(routes, parseRoute(r))
}

// Map with expected size
cache := make(map[string]Result, expectedSize)
```

### Avoid string concatenation in loops

```go
// BAD
var result string
for _, s := range items {
    result += s  // O(n²) allocations
}

// GOOD
var b strings.Builder
b.Grow(estimatedSize)  // Pre-allocate
for _, s := range items {
    b.WriteString(s)
}
result := b.String()
```

### Reuse buffers

```go
// BAD: New buffer each call
func format(r Route) []byte {
    buf := new(bytes.Buffer)
    fmt.Fprintf(buf, "%d:%s", r.VrfID, r.Prefix)
    return buf.Bytes()
}

// GOOD: Caller provides buffer
func format(buf *bytes.Buffer, r Route) {
    buf.Reset()
    fmt.Fprintf(buf, "%d:%s", r.VrfID, r.Prefix)
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Slice append in loop without capacity

```go
// BAD: Multiple reallocations
var routes []Route
for _, r := range input {
    routes = append(routes, r)  // Grows: 0→1→2→4→8→16...
}

// GOOD: Pre-allocate
routes := make([]Route, 0, len(input))
for _, r := range input {
    routes = append(routes, r)
}
```

### Pitfall 2: Map never shrinks

```go
cache := make(map[string]Data)

// Add 1 million entries
for i := 0; i < 1_000_000; i++ {
    cache[key(i)] = data(i)
}

// Delete all entries
for k := range cache {
    delete(cache, k)
}

// Memory is NOT freed! Map is still large.
// FIX: Create new map
cache = make(map[string]Data)
```

### Pitfall 3: Small slices preventing GC

```go
// BAD: Header keeps entire backing array alive
func getFirst(large []Route) Route {
    return large[:1][0]  // large's array can't be GC'd
}

// GOOD: Copy to break reference
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
    "bytes"
    "fmt"
    "runtime"
    "strings"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// BAD: Allocates on every call
func formatRoutesBad(routes []Route) string {
    var result string
    for _, r := range routes {
        result += fmt.Sprintf("%d:%s,", r.VrfID, r.Prefix)
    }
    return result
}

// GOOD: Pre-allocate, reuse buffer
func formatRoutesGood(routes []Route) string {
    var b strings.Builder
    // Estimate: ~30 chars per route
    b.Grow(len(routes) * 30)
    
    for i, r := range routes {
        if i > 0 {
            b.WriteByte(',')
        }
        fmt.Fprintf(&b, "%d:%s", r.VrfID, r.Prefix)
    }
    return b.String()
}

// BAD: Slice grows multiple times
func collectRoutesBad(n int) []Route {
    var routes []Route
    for i := 0; i < n; i++ {
        routes = append(routes, Route{VrfID: uint32(i)})
    }
    return routes
}

// GOOD: Pre-allocated
func collectRoutesGood(n int) []Route {
    routes := make([]Route, 0, n)
    for i := 0; i < n; i++ {
        routes = append(routes, Route{VrfID: uint32(i)})
    }
    return routes
}

// Buffer pool for reuse
type RouteFormatter struct {
    buf bytes.Buffer
}

func (f *RouteFormatter) Format(r Route) string {
    f.buf.Reset()
    fmt.Fprintf(&f.buf, "%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
    return f.buf.String()
}

func printMemStats(label string) {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    fmt.Printf("%s: Alloc=%dKB HeapObjects=%d\n",
        label, m.Alloc/1024, m.HeapObjects)
}

func main() {
    routes := make([]Route, 1000)
    for i := range routes {
        routes[i] = Route{
            VrfID:   uint32(i % 10),
            Prefix:  fmt.Sprintf("10.%d.%d.0/24", i/256, i%256),
            NextHop: "192.168.1.1",
        }
    }
    
    printMemStats("Initial")
    
    // Bad formatting
    _ = formatRoutesBad(routes)
    printMemStats("After bad format")
    
    runtime.GC()
    
    // Good formatting
    _ = formatRoutesGood(routes)
    printMemStats("After good format")
    
    runtime.GC()
    
    // Collection comparison
    _ = collectRoutesBad(10000)
    printMemStats("After bad collect")
    
    runtime.GC()
    
    _ = collectRoutesGood(10000)
    printMemStats("After good collect")
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ALLOCATION RULES                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PRE-ALLOCATE WHEN SIZE IS KNOWN                                    │
│      • make([]T, 0, n) for slices                                       │
│      • make(map[K]V, n) for maps                                        │
│      • strings.Builder.Grow(n)                                          │
│                                                                         │
│   2. AVOID STRING CONCATENATION IN LOOPS                                │
│      • Use strings.Builder                                              │
│      • Use bytes.Buffer                                                 │
│                                                                         │
│   3. REUSE BUFFERS                                                      │
│      • Pass buffer to functions                                         │
│      • Use sync.Pool for hot paths                                      │
│                                                                         │
│   4. BE AWARE OF HIDDEN ALLOCATIONS                                     │
│      • interface{} boxing                                               │
│      • String conversion                                                │
│      • Slice/map growth                                                 │
│                                                                         │
│   5. PROFILE BEFORE OPTIMIZING                                          │
│      • go test -bench -benchmem                                         │
│      • go build -gcflags="-m"                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 分配成本

| 类型 | 成本 | 说明 |
|------|------|------|
| 栈分配 | ~1 ns | 无 GC 开销 |
| 小堆分配 | ~25 ns | 需要 GC 追踪 |
| 大堆分配 | ~100+ ns | 可能触发 GC |

### 常见分配来源

- `make([]T, n)` - 切片底层数组
- `make(map[K]V)` - Map 结构
- `append()` - 超容量时重新分配
- 字符串拼接 - 创建新字符串
- `interface{}` - 装箱分配

### 优化技巧

1. **预分配**：`make([]T, 0, n)`
2. **避免循环中拼接字符串**：用 `strings.Builder`
3. **重用缓冲区**：传入 buffer 而非返回
4. **注意隐藏分配**：interface 装箱、字符串转换

### 最佳实践

- 知道大小就预分配
- 用 strings.Builder 拼接字符串
- sync.Pool 重用热路径对象
- 先测量后优化

