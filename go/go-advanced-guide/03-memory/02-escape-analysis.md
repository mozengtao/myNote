# Escape Analysis: Stack vs Heap

## 1. Engineering Problem

### What real-world problem does this solve?

**Go's compiler decides where to allocate memory - stack (fast) or heap (slower, GC-managed).**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STACK vs HEAP                                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   STACK                              HEAP                               │
│   ─────                              ────                               │
│   • Fast allocation (just move SP)   • Slower allocation                │
│   • Automatic cleanup on return      • Requires GC to cleanup           │
│   • Limited size (~1MB default)      • Large, growable                  │
│   • Local to goroutine               • Shared across goroutines         │
│                                                                         │
│   ┌─────────────────┐                ┌─────────────────┐                │
│   │   Function      │                │   Heap Memory   │                │
│   │   Stack Frame   │                │                 │                │
│   │ ┌─────────────┐ │                │  ┌───────────┐  │                │
│   │ │ local vars  │ │ ──Escapes───► │  │  Object   │  │                │
│   │ │ return addr │ │                │  └───────────┘  │                │
│   │ └─────────────┘ │                │                 │                │
│   └─────────────────┘                └─────────────────┘                │
│                                                                         │
│   Variable "escapes" to heap when:                                      │
│   • Returned as pointer from function                                   │
│   • Captured by closure that outlives function                          │
│   • Stored in interface{} or any                                        │
│   • Stored in global variable                                           │
│   • Too large for stack                                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Escape analysis rules

```go
// Does NOT escape - stays on stack
func stackAlloc() int {
    x := 42
    return x  // Value copied, x stays local
}

// ESCAPES to heap
func heapAlloc() *int {
    x := 42
    return &x  // Pointer returned, x must live after function returns
}
```

### When variables escape

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ESCAPE SCENARIOS                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. RETURN POINTER                                                     │
│      func new() *T { x := T{}; return &x }  // x escapes                │
│                                                                         │
│   2. STORE IN INTERFACE                                                 │
│      var i interface{} = x  // x escapes (interface boxes value)        │
│                                                                         │
│   3. CLOSURE CAPTURE                                                    │
│      go func() { use(x) }()  // x escapes if closure outlives func      │
│                                                                         │
│   4. STORE IN SLICE/MAP                                                 │
│      slice = append(slice, &x)  // x escapes                            │
│                                                                         │
│   5. TOO LARGE                                                          │
│      var big [1<<20]byte  // May escape if too large for stack          │
│                                                                         │
│   6. PASSED TO UNKNOWN FUNCTION                                         │
│      externalFunc(&x)  // x may escape (compiler can't prove safety)    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Viewing escape analysis

```bash
# Show escape analysis decisions
go build -gcflags="-m" main.go

# More verbose
go build -gcflags="-m -m" main.go

# Output examples:
# ./main.go:10:6: can inline foo
# ./main.go:15:9: &x escapes to heap
# ./main.go:20:6: x does not escape
```

### Examples with analysis

```go
// Does not escape
func sum(a, b int) int {
    result := a + b  // stays on stack
    return result
}

// Escapes to heap
func newRoute(prefix string) *Route {
    r := Route{Prefix: prefix}  // escapes
    return &r
}

// Escapes due to interface
func print(x interface{}) {  // x stored in interface
    fmt.Println(x)
}

func main() {
    n := 42
    print(n)  // n escapes to heap
}
```

---

## 4. Idiomatic Usage

### Prefer value returns when possible

```go
// GOOD: No heap allocation
func ParseRoute(s string) (Route, error) {
    return Route{Prefix: s}, nil
}

// LESS GOOD: Forces heap allocation
func ParseRoute(s string) (*Route, error) {
    return &Route{Prefix: s}, nil
}
```

### Pass pointers to avoid copying large structs

```go
// GOOD: Avoid copy of large struct
func processLarge(r *LargeStruct) {
    // Work with r
}

// BAD: Copies entire struct
func processLarge(r LargeStruct) {
    // r is a copy
}
```

### Pre-allocate to avoid escapes

```go
// BAD: Creates new slice each call, may escape
func process() []byte {
    buf := make([]byte, 1024)
    // use buf
    return buf
}

// GOOD: Caller provides buffer, no escape
func process(buf []byte) []byte {
    // use buf
    return buf[:n]
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Unnecessary pointer returns

```go
// BAD: Forces heap allocation
func newConfig() *Config {
    return &Config{Host: "localhost", Port: 8080}
}

// GOOD: Let caller decide
func newConfig() Config {
    return Config{Host: "localhost", Port: 8080}
}

// Caller can take address if needed
cfg := newConfig()
ptr := &cfg
```

### Pitfall 2: Interface boxing

```go
// Every call allocates because of interface{}
func logValue(v interface{}) {
    log.Println(v)
}

// Called in hot path - causes allocations
for _, route := range routes {
    logValue(route.VrfID)  // uint32 boxed to interface{}
}
```

### Pitfall 3: Closure capturing variables

```go
// BUG: All goroutines share same i
for i := 0; i < 10; i++ {
    go func() {
        fmt.Println(i)  // i escapes, race condition
    }()
}

// FIX: Pass as parameter
for i := 0; i < 10; i++ {
    go func(i int) {
        fmt.Println(i)  // i copied, no escape
    }(i)
}
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "runtime"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// Does not escape - value return
func createRouteValue() Route {
    return Route{
        VrfID:   1,
        Prefix:  "10.0.0.0/24",
        NextHop: "192.168.1.1",
    }
}

// Escapes to heap - pointer return
func createRoutePointer() *Route {
    return &Route{
        VrfID:   1,
        Prefix:  "10.0.0.0/24",
        NextHop: "192.168.1.1",
    }
}

// Escapes due to interface
func logRoute(r interface{}) {
    fmt.Printf("%+v\n", r)
}

// No escape - buffer passed in
func formatRoute(buf []byte, r Route) []byte {
    return fmt.Appendf(buf, "%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
}

func printMemStats(label string) {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    fmt.Printf("%s: Alloc=%d KB, HeapObjects=%d\n",
        label, m.Alloc/1024, m.HeapObjects)
}

func main() {
    printMemStats("Start")
    
    // Value - no heap allocation
    for i := 0; i < 1000; i++ {
        r := createRouteValue()
        _ = r
    }
    printMemStats("After value creates")
    
    // Pointer - heap allocations
    routes := make([]*Route, 0, 1000)
    for i := 0; i < 1000; i++ {
        routes = append(routes, createRoutePointer())
    }
    printMemStats("After pointer creates")
    
    // Reusing buffer
    buf := make([]byte, 0, 256)
    for i := 0; i < 1000; i++ {
        buf = formatRoute(buf[:0], Route{VrfID: uint32(i)})
    }
    printMemStats("After buffer reuse")
    
    runtime.GC()
    printMemStats("After GC")
}
```

Run with escape analysis:
```bash
go build -gcflags="-m" main.go 2>&1 | grep escape
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ESCAPE ANALYSIS RULES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. PREFER VALUE RETURNS FOR SMALL TYPES                               │
│      • Lets compiler optimize allocation                                │
│      • Caller can take address if needed                                │
│                                                                         │
│   2. AVOID UNNECESSARY interface{}                                      │
│      • Causes boxing/allocation                                         │
│      • Use generics (Go 1.18+) in hot paths                            │
│                                                                         │
│   3. PASS BUFFERS TO FUNCTIONS                                          │
│      • Avoids escape in hot paths                                       │
│      • Enables buffer reuse                                             │
│                                                                         │
│   4. USE -gcflags="-m" TO ANALYZE                                       │
│      • Understand where allocations happen                              │
│      • Optimize only hot paths                                          │
│                                                                         │
│   5. TRUST THE COMPILER                                                 │
│      • Escape analysis is sophisticated                                 │
│      • Profile before optimizing                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 逃逸分析核心概念

**Go 编译器决定变量分配在栈上（快）还是堆上（慢，需要 GC）。**

### 栈 vs 堆

| 特性 | 栈 | 堆 |
|------|-----|-----|
| 分配速度 | 快（移动栈指针） | 慢 |
| 清理 | 函数返回自动清理 | 需要 GC |
| 大小 | 有限（~1MB） | 可增长 |
| 作用域 | goroutine 本地 | 共享 |

### 逃逸场景

1. **返回指针**：`return &x`
2. **存入接口**：`var i interface{} = x`
3. **闭包捕获**：`go func() { use(x) }()`
4. **存入 slice/map**：`slice = append(slice, &x)`
5. **太大**：大数组
6. **传给外部函数**：编译器无法证明安全

### 查看逃逸分析

```bash
go build -gcflags="-m" main.go
```

### 最佳实践

1. **小类型返回值**：让编译器优化
2. **避免不必要的 interface{}**：导致装箱分配
3. **传入缓冲区**：避免逃逸
4. **先分析后优化**：只优化热点路径

