# Race Detector: Finding Data Races

## 1. Engineering Problem

### What real-world problem does this solve?

**Data races are among the hardest bugs to find. The race detector finds them at runtime.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DATA RACE                                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Goroutine 1:              Goroutine 2:                                │
│   ────────────              ────────────                                │
│                                                                         │
│   counter++                 counter++                                   │
│        │                         │                                      │
│        └─────────┬───────────────┘                                      │
│                  │                                                      │
│             DATA RACE!                                                  │
│   Unsynchronized concurrent access to same memory                       │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Why it's dangerous:                                                   │
│   • May work 99% of the time                                           │
│   • Fails randomly under load                                          │
│   • Hard to reproduce                                                  │
│   • Can cause data corruption                                          │
│   • May behave differently per CPU/OS                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Bugs appear only in production under load
- Tests pass locally, fail in CI
- Data corruption with no obvious cause
- Intermittent crashes that can't be reproduced

---

## 2. Core Mental Model

### How Go expects you to think

**A data race occurs when two goroutines access the same variable and at least one is a write.**

```go
// DATA RACE - two goroutines, unsynchronized
go func() { x = 1 }()      // Write
go func() { fmt.Println(x) }()  // Read
```

### No race when:

- Only reads (multiple readers OK)
- Synchronized with mutex
- Synchronized with channels
- Using atomic operations

### Philosophy

- Run `-race` in CI, every time
- Fix all races, no exceptions
- Race detector finds races, not all concurrency bugs

---

## 3. Language Mechanism

### Using the race detector

```bash
# Test with race detector
go test -race ./...

# Build with race detector
go build -race -o myapp

# Run with race detector
./myapp  # (built with -race)
```

### Race detector output

```
WARNING: DATA RACE
Write at 0x00c0000a4000 by goroutine 7:
  main.increment()
      main.go:10 +0x38

Previous read at 0x00c0000a4000 by goroutine 6:
  main.read()
      main.go:15 +0x30

Goroutine 7 (running) created at:
  main.main()
      main.go:20 +0x68

Goroutine 6 (finished) created at:
  main.main()
      main.go:19 +0x50
```

### What it tells you:

1. Type of access (read/write)
2. Memory address
3. Goroutine that accessed
4. Stack trace
5. Where goroutines were created

---

## 4. Idiomatic Usage

### When to use

- **Always in CI/CD pipelines**
- During development
- In pre-production testing
- Never in production (performance impact)

### Fixing races

```go
// Option 1: Mutex
var mu sync.Mutex
var counter int

func increment() {
    mu.Lock()
    counter++
    mu.Unlock()
}

// Option 2: Atomic
var counter int64

func increment() {
    atomic.AddInt64(&counter, 1)
}

// Option 3: Channel
func worker(in <-chan int, out chan<- int) {
    for v := range in {
        out <- v * 2
    }
}
```

### Pattern: Race-free counter

```go
type Counter struct {
    mu sync.Mutex
    n  int
}

func (c *Counter) Inc() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.n++
}

func (c *Counter) Get() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.n
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Map without mutex

```go
// BAD: Concurrent map access
var cache = make(map[string]Result)

go func() { cache["a"] = result1 }()
go func() { cache["b"] = result2 }()  // RACE!

// GOOD: Use sync.Map or mutex
var cache sync.Map

go func() { cache.Store("a", result1) }()
go func() { cache.Store("b", result2) }()  // Safe
```

### Pitfall 2: Closure capturing loop variable

```go
// BAD: All goroutines share same i
for i := 0; i < 10; i++ {
    go func() {
        fmt.Println(i)  // RACE on i!
    }()
}

// GOOD: Capture i
for i := 0; i < 10; i++ {
    i := i  // Shadow i
    go func() {
        fmt.Println(i)  // Safe
    }()
}
```

### Pitfall 3: Assuming small reads are safe

```go
// BAD: Even bool can race
var done bool

go func() { done = true }()
go func() {
    if done { ... }  // RACE!
}()

// GOOD: Use atomic or channel
var done atomic.Bool

go func() { done.Store(true) }()
go func() {
    if done.Load() { ... }  // Safe
}()
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
)

// =====================
// BAD: Has data race
// =====================
type UnsafeRouteManager struct {
    routes map[string]Route
}

func (rm *UnsafeRouteManager) Add(r Route) {
    rm.routes[r.Prefix] = r  // RACE!
}

func (rm *UnsafeRouteManager) Get(prefix string) (Route, bool) {
    r, ok := rm.routes[prefix]  // RACE!
    return r, ok
}

// =====================
// GOOD: Race-free with mutex
// =====================
type SafeRouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewSafeRouteManager() *SafeRouteManager {
    return &SafeRouteManager{
        routes: make(map[string]Route),
    }
}

func (rm *SafeRouteManager) Add(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    rm.routes[r.Prefix] = r
}

func (rm *SafeRouteManager) Get(prefix string) (Route, bool) {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    r, ok := rm.routes[prefix]
    return r, ok
}

// =====================
// GOOD: Race-free counter with atomic
// =====================
type Stats struct {
    gets  atomic.Int64
    adds  atomic.Int64
}

func (s *Stats) RecordGet() {
    s.gets.Add(1)
}

func (s *Stats) RecordAdd() {
    s.adds.Add(1)
}

func (s *Stats) Report() {
    fmt.Printf("Gets: %d, Adds: %d\n", s.gets.Load(), s.adds.Load())
}

// =====================
// Test that would catch race
// =====================
type Route struct {
    Prefix  string
    NextHop string
}

func TestRaceCondition(t *testing.T) {
    rm := NewSafeRouteManager()
    
    var wg sync.WaitGroup
    
    // Multiple writers
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(i int) {
            defer wg.Done()
            rm.Add(Route{
                Prefix:  fmt.Sprintf("10.0.%d.0/24", i),
                NextHop: "192.168.1.1",
            })
        }(i)
    }
    
    // Multiple readers
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(i int) {
            defer wg.Done()
            rm.Get(fmt.Sprintf("10.0.%d.0/24", i))
        }(i)
    }
    
    wg.Wait()
}

func main() {
    rm := NewSafeRouteManager()
    stats := &Stats{}
    
    var wg sync.WaitGroup
    
    // Concurrent adds and gets
    for i := 0; i < 100; i++ {
        wg.Add(2)
        
        go func(i int) {
            defer wg.Done()
            rm.Add(Route{
                Prefix:  fmt.Sprintf("10.0.%d.0/24", i),
                NextHop: "192.168.1.1",
            })
            stats.RecordAdd()
        }(i)
        
        go func(i int) {
            defer wg.Done()
            rm.Get(fmt.Sprintf("10.0.%d.0/24", i))
            stats.RecordGet()
        }(i)
    }
    
    wg.Wait()
    stats.Report()
}

// Need to add for compilation
import "testing"
```

Test with race detector:
```bash
go test -race -run TestRaceCondition
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    RACE DETECTOR RULES                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. RUN -race IN CI, ALWAYS                                            │
│      • go test -race ./...                                              │
│      • Catches races before production                                  │
│                                                                         │
│   2. FIX ALL REPORTED RACES                                             │
│      • No "benign" races                                                │
│      • Races are undefined behavior                                     │
│                                                                         │
│   3. DON'T RUN -race IN PRODUCTION                                      │
│      • 10x slower                                                       │
│      • 5-10x more memory                                                │
│                                                                         │
│   4. PROTECT SHARED STATE                                               │
│      • sync.Mutex for complex state                                     │
│      • sync.RWMutex for read-heavy                                      │
│      • atomic for simple counters                                       │
│      • Channels for ownership transfer                                  │
│                                                                         │
│   5. COMMON SOURCES                                                     │
│      • Maps without mutex                                               │
│      • Loop variables in closures                                       │
│      • Struct fields accessed concurrently                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 数据竞争

**两个 goroutine 访问同一变量，至少一个是写操作。**

### 使用竞态检测器

```bash
go test -race ./...  # 测试时检测
go build -race       # 构建时启用
```

### 常见竞争来源

| 来源 | 解决方案 |
|------|----------|
| 无锁 map | sync.Map 或 mutex |
| 循环变量闭包 | 捕获变量 `i := i` |
| 结构体字段 | 用 mutex 保护 |
| 简单计数器 | 用 atomic |

### 最佳实践

1. **CI 中必须用 -race**
2. **修复所有竞争**：没有"良性"竞争
3. **生产环境不用 -race**：性能影响大
4. **保护共享状态**：mutex、atomic、channel
