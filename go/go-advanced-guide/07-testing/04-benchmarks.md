# Benchmarks: Performance Testing in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**Benchmarks measure performance objectively and detect regressions before production.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PERFORMANCE MEASUREMENT                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without benchmarks:                With benchmarks:                   │
│   ───────────────────                ─────────────────                  │
│                                                                         │
│   "I think it's faster"              BenchmarkOld: 500 ns/op            │
│   "It feels slow"                    BenchmarkNew: 300 ns/op            │
│   "Let's optimize this"              Improvement: 40%                   │
│                                                                         │
│   • Subjective                       • Objective                        │
│   • Can't compare                    • Reproducible                     │
│   • Guessing                         • Evidence-based                   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Benchmark output:                                                     │
│                                                                         │
│   BenchmarkParseRoute-8   5000000   300 ns/op   48 B/op   2 allocs/op  │
│        │              │       │         │           │           │       │
│        │              │       │         │           │           │       │
│        name           CPUs    iters     time       mem        allocs    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Optimizing code that doesn't matter (not on hot path)
- Benchmark doesn't reflect real usage patterns
- Compiler optimizes away the code being measured
- Results vary wildly between runs

---

## 2. Core Mental Model

### How Go expects you to think

**Run the code b.N times. Go adjusts b.N until timing is stable.**

```go
func BenchmarkFunction(b *testing.B) {
    // Setup (not measured)
    data := prepareData()
    
    b.ResetTimer()  // Start measuring
    
    for i := 0; i < b.N; i++ {
        Function(data)  // Measured code
    }
}
```

### Philosophy

- Measure before optimizing
- Compare before and after
- Use realistic inputs
- Run multiple times for stability

---

## 3. Language Mechanism

### Basic benchmark

```go
func BenchmarkParseRoute(b *testing.B) {
    input := "10.0.0.0/24"
    
    for i := 0; i < b.N; i++ {
        ParseRoute(input)
    }
}
```

### With memory allocation reporting

```go
func BenchmarkRouteManager(b *testing.B) {
    b.ReportAllocs()  // Include allocation stats
    
    for i := 0; i < b.N; i++ {
        rm := NewRouteManager()
        rm.Add(Route{Prefix: "10.0.0.0/24"})
    }
}
```

### With setup/teardown

```go
func BenchmarkComplexOperation(b *testing.B) {
    // Setup (not measured)
    db := setupDatabase()
    defer db.Close()
    
    routes := generateRoutes(1000)
    
    b.ResetTimer()  // Reset timer after setup
    
    for i := 0; i < b.N; i++ {
        processRoutes(routes)
    }
}
```

### Sub-benchmarks

```go
func BenchmarkParse(b *testing.B) {
    sizes := []int{10, 100, 1000}
    
    for _, size := range sizes {
        b.Run(fmt.Sprintf("size-%d", size), func(b *testing.B) {
            input := generateInput(size)
            b.ResetTimer()
            
            for i := 0; i < b.N; i++ {
                Parse(input)
            }
        })
    }
}
```

### Running benchmarks

```bash
# Run all benchmarks
go test -bench=.

# Run with memory stats
go test -bench=. -benchmem

# Run specific benchmark
go test -bench=BenchmarkParseRoute

# Run multiple times
go test -bench=. -count=5

# Longer duration
go test -bench=. -benchtime=5s

# CPU profile
go test -bench=. -cpuprofile=cpu.prof
```

---

## 4. Idiomatic Usage

### When to benchmark

- Hot paths (frequently called code)
- Before and after optimization
- When choosing between implementations
- Performance-sensitive code

### When NOT to benchmark

- Code that runs rarely
- I/O-bound operations (network, disk)
- Before you have working code

### Pattern: Comparison benchmark

```go
func BenchmarkStringConcat(b *testing.B) {
    b.Run("plus", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            _ = "hello" + " " + "world"
        }
    })
    
    b.Run("sprintf", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            _ = fmt.Sprintf("%s %s", "hello", "world")
        }
    })
    
    b.Run("builder", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            var sb strings.Builder
            sb.WriteString("hello")
            sb.WriteString(" ")
            sb.WriteString("world")
            _ = sb.String()
        }
    })
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Compiler optimizes away code

```go
// BAD: Result unused, compiler may eliminate
func BenchmarkParse(b *testing.B) {
    for i := 0; i < b.N; i++ {
        ParseRoute("10.0.0.0/24")  // May be optimized out!
    }
}

// GOOD: Use result
var result Route

func BenchmarkParse(b *testing.B) {
    var r Route
    for i := 0; i < b.N; i++ {
        r = ParseRoute("10.0.0.0/24")
    }
    result = r  // Prevent optimization
}
```

### Pitfall 2: Including setup in measurement

```go
// BAD: Setup time included
func BenchmarkProcess(b *testing.B) {
    for i := 0; i < b.N; i++ {
        data := generateData(1000)  // Setup in loop!
        Process(data)
    }
}

// GOOD: Setup outside loop
func BenchmarkProcess(b *testing.B) {
    data := generateData(1000)  // Setup once
    b.ResetTimer()
    
    for i := 0; i < b.N; i++ {
        Process(data)
    }
}
```

### Pitfall 3: Benchmark with shared state

```go
// BAD: State accumulates
func BenchmarkAdd(b *testing.B) {
    rm := NewRouteManager()
    
    for i := 0; i < b.N; i++ {
        rm.Add(Route{Prefix: fmt.Sprintf("%d.0.0.0/24", i)})
        // Manager grows each iteration!
    }
}

// GOOD: Fresh state each iteration
func BenchmarkAdd(b *testing.B) {
    for i := 0; i < b.N; i++ {
        rm := NewRouteManager()
        rm.Add(Route{Prefix: "10.0.0.0/24"})
    }
}
```

---

## 6. Complete, Realistic Example

```go
package route

import (
    "fmt"
    "strings"
    "sync"
    "testing"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewRouteManager() *RouteManager {
    return &RouteManager{routes: make(map[string]Route)}
}

func (rm *RouteManager) Add(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    rm.routes[r.Prefix] = r
}

func (rm *RouteManager) Get(prefix string) (Route, bool) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    r, ok := rm.routes[prefix]
    return r, ok
}

// Format implementations to compare
func formatPlus(r Route) string {
    return fmt.Sprintf("%d", r.VrfID) + ":" + r.Prefix + "->" + r.NextHop
}

func formatSprintf(r Route) string {
    return fmt.Sprintf("%d:%s->%s", r.VrfID, r.Prefix, r.NextHop)
}

func formatBuilder(r Route) string {
    var sb strings.Builder
    sb.WriteString(fmt.Sprintf("%d", r.VrfID))
    sb.WriteString(":")
    sb.WriteString(r.Prefix)
    sb.WriteString("->")
    sb.WriteString(r.NextHop)
    return sb.String()
}

// Prevent compiler optimization
var benchResult string
var benchRoute Route

// Basic benchmark
func BenchmarkRouteManagerAdd(b *testing.B) {
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    
    for i := 0; i < b.N; i++ {
        rm := NewRouteManager()
        rm.Add(route)
    }
}

// Benchmark with memory allocation
func BenchmarkRouteManagerAddWithAllocs(b *testing.B) {
    b.ReportAllocs()
    
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    
    for i := 0; i < b.N; i++ {
        rm := NewRouteManager()
        rm.Add(route)
    }
}

// Benchmark Get operation
func BenchmarkRouteManagerGet(b *testing.B) {
    rm := NewRouteManager()
    for i := 0; i < 1000; i++ {
        rm.Add(Route{
            VrfID:   1,
            Prefix:  fmt.Sprintf("10.0.%d.0/24", i),
            NextHop: "192.168.1.1",
        })
    }
    
    b.ResetTimer()
    
    var r Route
    for i := 0; i < b.N; i++ {
        r, _ = rm.Get("10.0.500.0/24")
    }
    benchRoute = r
}

// Compare formatting implementations
func BenchmarkFormat(b *testing.B) {
    route := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    
    b.Run("plus", func(b *testing.B) {
        var s string
        for i := 0; i < b.N; i++ {
            s = formatPlus(route)
        }
        benchResult = s
    })
    
    b.Run("sprintf", func(b *testing.B) {
        var s string
        for i := 0; i < b.N; i++ {
            s = formatSprintf(route)
        }
        benchResult = s
    })
    
    b.Run("builder", func(b *testing.B) {
        var s string
        for i := 0; i < b.N; i++ {
            s = formatBuilder(route)
        }
        benchResult = s
    })
}

// Benchmark with varying sizes
func BenchmarkRouteManagerSize(b *testing.B) {
    sizes := []int{10, 100, 1000, 10000}
    
    for _, size := range sizes {
        b.Run(fmt.Sprintf("size-%d", size), func(b *testing.B) {
            rm := NewRouteManager()
            for i := 0; i < size; i++ {
                rm.Add(Route{
                    VrfID:   1,
                    Prefix:  fmt.Sprintf("10.%d.%d.0/24", i/256, i%256),
                    NextHop: "192.168.1.1",
                })
            }
            
            b.ResetTimer()
            
            var r Route
            for i := 0; i < b.N; i++ {
                r, _ = rm.Get("10.0.50.0/24")
            }
            benchRoute = r
        })
    }
}

// Parallel benchmark
func BenchmarkRouteManagerGetParallel(b *testing.B) {
    rm := NewRouteManager()
    for i := 0; i < 1000; i++ {
        rm.Add(Route{
            VrfID:   1,
            Prefix:  fmt.Sprintf("10.0.%d.0/24", i),
            NextHop: "192.168.1.1",
        })
    }
    
    b.ResetTimer()
    b.RunParallel(func(pb *testing.PB) {
        var r Route
        for pb.Next() {
            r, _ = rm.Get("10.0.500.0/24")
        }
        benchRoute = r
    })
}
```

Run:
```bash
go test -bench=. -benchmem

# Output example:
# BenchmarkRouteManagerAdd-8              5000000    290 ns/op   336 B/op   3 allocs/op
# BenchmarkFormat/plus-8                  10000000   180 ns/op    64 B/op   3 allocs/op
# BenchmarkFormat/sprintf-8               5000000    320 ns/op    64 B/op   3 allocs/op
# BenchmarkFormat/builder-8               5000000    350 ns/op   112 B/op   4 allocs/op
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    BENCHMARK RULES                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE b.ResetTimer() AFTER SETUP                                     │
│      • Setup code not included in timing                                │
│      • Reset timer after preparing data                                 │
│                                                                         │
│   2. PREVENT COMPILER OPTIMIZATION                                      │
│      • Store result in package-level var                                │
│      • Use the result somehow                                           │
│                                                                         │
│   3. USE b.ReportAllocs() FOR MEMORY                                    │
│      • Shows allocations per operation                                  │
│      • Also use -benchmem flag                                          │
│                                                                         │
│   4. RUN MULTIPLE TIMES                                                 │
│      • go test -bench=. -count=5                                        │
│      • Look for consistency                                             │
│                                                                         │
│   5. COMPARE BEFORE AND AFTER                                           │
│      • Save baseline results                                            │
│      • Use benchstat for comparison                                     │
│                                                                         │
│   6. BENCHMARK REALISTIC SCENARIOS                                      │
│      • Real input sizes                                                 │
│      • Real usage patterns                                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 基准测试基本结构

```go
func BenchmarkFunction(b *testing.B) {
    // 设置（不计时）
    data := prepare()
    b.ResetTimer()
    
    for i := 0; i < b.N; i++ {
        Function(data)  // 被测代码
    }
}
```

### 输出解读

```
BenchmarkX-8   5000000   300 ns/op   48 B/op   2 allocs/op
     │             │         │           │           │
     名称        迭代次数   每次耗时    每次内存   每次分配
```

### 常见陷阱

| 陷阱 | 问题 | 解决方案 |
|------|------|----------|
| 结果未使用 | 编译器优化掉代码 | 存储到包级变量 |
| 设置在循环内 | 计时不准确 | 设置在循环外 |
| 状态累积 | 每次迭代不同 | 每次迭代重新创建 |

### 最佳实践

1. 用 `b.ResetTimer()` 排除设置时间
2. 用包级变量防止优化
3. 用 `b.ReportAllocs()` 报告内存
4. 多次运行 (`-count=5`)
5. 比较优化前后
