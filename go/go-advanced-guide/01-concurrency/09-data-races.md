# Data Races and the Race Detector

## 1. Engineering Problem

### What real-world problem does this solve?

**A data race is the most insidious concurrency bug: undefined behavior that may work 99.9% of the time.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      THE DATA RACE PROBLEM                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Definition: A data race occurs when:                                  │
│   1. Two or more goroutines access the same memory location             │
│   2. At least one is a write                                            │
│   3. There's no synchronization between them                            │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Time ──────────────────────────────────────────────────────────────►  │
│                                                                         │
│   Goroutine A:    read x ───────────────► write x                       │
│                      │                        │                         │
│                      │     RACE WINDOW        │                         │
│                      │                        │                         │
│   Goroutine B:       └─────── write x ────────┘                         │
│                                                                         │
│   What value does x have? UNDEFINED.                                    │
│   • Might see old value                                                 │
│   • Might see new value                                                 │
│   • Might see partial/corrupted value                                   │
│   • Behavior may differ between runs, CPUs, Go versions                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Why are data races so dangerous?

1. **Hard to reproduce**: May only fail under specific timing
2. **Silent corruption**: No crash, just wrong results
3. **Heisenbugs**: Adding debugging often changes timing, hiding the bug
4. **Portability issues**: Works on your laptop, fails in production
5. **Undefined behavior**: Compiler can optimize in surprising ways

---

## 2. Core Mental Model

### The happens-before relationship

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    HAPPENS-BEFORE MODEL                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   "A happens-before B" means A's effects are visible to B               │
│                                                                         │
│   Synchronized (SAFE):                                                  │
│   ────────────────────                                                  │
│                                                                         │
│   Goroutine A:   write x ───►│ channel send   │                         │
│                              │                │                         │
│                              │ happens-before │                         │
│                              │                │                         │
│   Goroutine B:               │ channel recv   │◄─── read x              │
│                                                                         │
│   The send happens-before the receive                                   │
│   Therefore, the write happens-before the read (SAFE)                   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Unsynchronized (DATA RACE):                                           │
│   ───────────────────────────                                           │
│                                                                         │
│   Goroutine A:   write x ─────────────────────────                      │
│                              │                                          │
│                              │ No ordering!                             │
│                              │                                          │
│   Goroutine B:   ────────────┼──────────────── read x                   │
│                                                                         │
│   No synchronization = no happens-before = DATA RACE                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What creates happens-before?

```go
// 1. Channel send happens-before receive completes
ch <- x     // happens-before
y := <-ch   // this

// 2. Mutex unlock happens-before next lock
mu.Unlock() // happens-before
mu.Lock()   // this

// 3. sync.WaitGroup Done happens-before Wait returns
wg.Done()   // happens-before
wg.Wait()   // this returns

// 4. sync.Once Do happens-before any Do returns
once.Do(f)  // f() happens-before
once.Do(g)  // this returns (g not called)

// 5. Goroutine creation happens-before goroutine starts
x = 1       // happens-before
go func() {
    use(x)  // this
}()
```

---

## 3. Common Race Conditions

### Race 1: Shared counter

```go
// RACE: Multiple goroutines incrementing without sync
var counter int

for i := 0; i < 1000; i++ {
    go func() {
        counter++  // READ-MODIFY-WRITE is not atomic!
    }()
}

// FIX 1: Mutex
var mu sync.Mutex
var counter int

for i := 0; i < 1000; i++ {
    go func() {
        mu.Lock()
        counter++
        mu.Unlock()
    }()
}

// FIX 2: Atomic
var counter atomic.Int64

for i := 0; i < 1000; i++ {
    go func() {
        counter.Add(1)
    }()
}
```

### Race 2: Check-then-act

```go
// RACE: Time-of-check to time-of-use (TOCTOU)
func (m *Map) GetOrCreate(key string) *Value {
    if v, ok := m.data[key]; ok {  // CHECK
        return v
    }
    // Another goroutine might create key here!
    v := &Value{}
    m.data[key] = v  // ACT
    return v
}

// FIX: Hold lock for entire operation
func (m *Map) GetOrCreate(key string) *Value {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    if v, ok := m.data[key]; ok {
        return v
    }
    v := &Value{}
    m.data[key] = v
    return v
}
```

### Race 3: Loop variable capture

```go
// RACE: All goroutines share the same loop variable
for _, item := range items {
    go func() {
        process(item)  // item changes while goroutine runs!
    }()
}

// FIX: Pass as parameter (copy)
for _, item := range items {
    go func(it Item) {
        process(it)
    }(item)
}

// FIX (Go 1.22+): Loop variable is now per-iteration
// Still good practice to be explicit for clarity
```

### Race 4: Lazy initialization

```go
// RACE: Multiple goroutines may initialize
var instance *Config

func GetConfig() *Config {
    if instance == nil {  // RACE: read
        instance = loadConfig()  // RACE: write
    }
    return instance
}

// FIX: Use sync.Once
var (
    instance *Config
    once     sync.Once
)

func GetConfig() *Config {
    once.Do(func() {
        instance = loadConfig()
    })
    return instance
}
```

### Race 5: Slice/map concurrent access

```go
// RACE: Maps are not safe for concurrent write
m := make(map[string]int)

go func() { m["a"] = 1 }()
go func() { m["b"] = 2 }()  // RACE!

// FIX: Use mutex or sync.Map
var mu sync.Mutex
m := make(map[string]int)

go func() {
    mu.Lock()
    m["a"] = 1
    mu.Unlock()
}()
```

---

## 4. The Race Detector

### How to use it

```bash
# Run tests with race detector
go test -race ./...

# Build with race detector
go build -race

# Run with race detector
go run -race main.go
```

### What it reports

```
==================
WARNING: DATA RACE
Read at 0x00c0000a4008 by goroutine 7:
  main.main.func1()
      /path/main.go:15 +0x64

Previous write at 0x00c0000a4008 by goroutine 6:
  main.main.func2()
      /path/main.go:12 +0x84

Goroutine 7 (running) created at:
  main.main()
      /path/main.go:14 +0x98

Goroutine 6 (finished) created at:
  main.main()
      /path/main.go:11 +0x78
==================
```

### Race detector in CI

```yaml
# .github/workflows/test.yml
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-go@v4
        with:
          go-version: '1.22'
      - name: Test with race detector
        run: go test -race -v ./...
```

### Race detector limitations

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 RACE DETECTOR LIMITATIONS                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   What it CAN do:                                                       │
│   • Detect races that actually happen during execution                  │
│   • Zero false positives (if it reports, it's a race)                   │
│                                                                         │
│   What it CANNOT do:                                                    │
│   • Detect races that didn't execute (need good test coverage)          │
│   • Find races in code paths not taken                                  │
│   • Guarantee race-free code                                            │
│                                                                         │
│   Performance impact:                                                   │
│   • 5-10x slower execution                                              │
│   • 5-10x more memory                                                   │
│   • Don't use in production (except for debugging)                      │
│                                                                         │
│   Best practice:                                                        │
│   • Always run tests with -race in CI                                   │
│   • High test coverage = better race detection                          │
│   • Run load tests with -race occasionally                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Complete Example: Race Detection Demo

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
    "time"
)

// DemonstrateRaces shows common race patterns
func main() {
    fmt.Println("=== Data Race Demo ===")
    fmt.Println("Run with: go run -race main.go")
    fmt.Println()
    
    // Example 1: Counter race
    demonstrateCounterRace()
    
    // Example 2: Safe counter
    demonstrateSafeCounter()
    
    // Example 3: Map race
    demonstrateMapRace()
    
    // Example 4: Safe map
    demonstrateSafeMap()
    
    fmt.Println("\n=== Demo Complete ===")
}

func demonstrateCounterRace() {
    fmt.Println("--- Counter Race (UNSAFE) ---")
    
    var counter int
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                counter++  // RACE!
            }
        }()
    }
    
    wg.Wait()
    fmt.Printf("Counter (expected 10000): %d\n", counter)
}

func demonstrateSafeCounter() {
    fmt.Println("\n--- Safe Counter (atomic) ---")
    
    var counter atomic.Int64
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                counter.Add(1)  // Safe
            }
        }()
    }
    
    wg.Wait()
    fmt.Printf("Counter (expected 10000): %d\n", counter.Load())
}

func demonstrateMapRace() {
    fmt.Println("\n--- Map Race (UNSAFE) ---")
    
    m := make(map[int]int)
    var wg sync.WaitGroup
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                m[id*100+j] = j  // RACE!
            }
        }(i)
    }
    
    wg.Wait()
    fmt.Printf("Map size: %d\n", len(m))
}

func demonstrateSafeMap() {
    fmt.Println("\n--- Safe Map (mutex) ---")
    
    var mu sync.Mutex
    m := make(map[int]int)
    var wg sync.WaitGroup
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                mu.Lock()
                m[id*100+j] = j
                mu.Unlock()
            }
        }(i)
    }
    
    wg.Wait()
    fmt.Printf("Map size: %d\n", len(m))
}

// SafeCache demonstrates proper concurrent map access
type SafeCache struct {
    mu    sync.RWMutex
    items map[string]interface{}
}

func NewSafeCache() *SafeCache {
    return &SafeCache{
        items: make(map[string]interface{}),
    }
}

func (c *SafeCache) Get(key string) (interface{}, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    v, ok := c.items[key]
    return v, ok
}

func (c *SafeCache) Set(key string, value interface{}) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.items[key] = value
}

// GetOrCreate demonstrates atomic check-then-act
func (c *SafeCache) GetOrCreate(key string, create func() interface{}) interface{} {
    // Try read-only first (optimization)
    c.mu.RLock()
    if v, ok := c.items[key]; ok {
        c.mu.RUnlock()
        return v
    }
    c.mu.RUnlock()
    
    // Need to create - acquire write lock
    c.mu.Lock()
    defer c.mu.Unlock()
    
    // Double-check (another goroutine might have created it)
    if v, ok := c.items[key]; ok {
        return v
    }
    
    v := create()
    c.items[key] = v
    return v
}

// RouteState demonstrates proper state management (from routermgr context)
type RouteState struct {
    mu       sync.RWMutex
    routes   map[string]Route
    version  atomic.Int64
    
    // Separate locks for independent operations
    addrMu   sync.Mutex
    addresses map[string]Address
}

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type Address struct {
    IP     string
    Prefix int
}

func NewRouteState() *RouteState {
    return &RouteState{
        routes:    make(map[string]Route),
        addresses: make(map[string]Address),
    }
}

func (rs *RouteState) AddRoute(key string, route Route) {
    rs.mu.Lock()
    defer rs.mu.Unlock()
    
    rs.routes[key] = route
    rs.version.Add(1)
}

func (rs *RouteState) GetRoute(key string) (Route, bool) {
    rs.mu.RLock()
    defer rs.mu.RUnlock()
    
    r, ok := rs.routes[key]
    return r, ok
}

func (rs *RouteState) AddAddress(key string, addr Address) {
    rs.addrMu.Lock()
    defer rs.addrMu.Unlock()
    
    rs.addresses[key] = addr
}

func (rs *RouteState) Version() int64 {
    return rs.version.Load()
}

// Demonstrate concurrent safe usage
func demonstrateSafeState() {
    state := NewRouteState()
    var wg sync.WaitGroup
    
    // Writers
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                key := fmt.Sprintf("%d:%d", id, j)
                state.AddRoute(key, Route{
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
                key := fmt.Sprintf("%d:%d", id%10, j%100)
                state.GetRoute(key)
                time.Sleep(time.Microsecond)
            }
        }(i)
    }
    
    wg.Wait()
    fmt.Printf("Final version: %d\n", state.Version())
}
```

---

## 6. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DATA RACE PREVENTION RULES                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ALWAYS run tests with -race in CI                                  │
│      go test -race ./...                                                │
│                                                                         │
│   2. Protect shared state with ONE of:                                  │
│      • sync.Mutex / sync.RWMutex                                        │
│      • sync/atomic for simple values                                    │
│      • Channels for ownership transfer                                  │
│                                                                         │
│   3. Hold lock for entire check-then-act sequence                       │
│      Don't release and re-acquire                                       │
│                                                                         │
│   4. Document what each mutex protects                                  │
│      // mu protects routes and version                                  │
│      var mu sync.Mutex                                                  │
│                                                                         │
│   5. Prefer immutability where possible                                 │
│      Return copies, not references to internal state                    │
│                                                                         │
│   6. Be explicit about ownership                                        │
│      If you send on a channel, you no longer own it                     │
│                                                                         │
│   7. Test concurrent code paths                                         │
│      Race detector only catches races that execute                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 数据竞争定义

**当两个或多个 goroutine 同时访问同一内存位置，且至少一个是写操作，且没有同步机制时，就发生了数据竞争。**

### 数据竞争的危害

1. **难以重现**：可能只在特定时序下失败
2. **静默损坏**：没有崩溃，只是结果错误
3. **海森堡 Bug**：加调试代码会改变时序，隐藏问题
4. **可移植性问题**：在你的笔记本上工作，生产环境失败
5. **未定义行为**：编译器可能进行意外的优化

### happens-before 关系

以下操作建立 happens-before 关系：

| 操作 A | happens-before | 操作 B |
|--------|----------------|--------|
| channel 发送 | → | channel 接收完成 |
| mutex 解锁 | → | 下一次 mutex 加锁 |
| WaitGroup.Done() | → | Wait() 返回 |
| sync.Once.Do(f) | → | 任何 Do() 返回 |
| 创建 goroutine | → | goroutine 开始执行 |

### Race Detector 使用

```bash
# 运行测试时检测竞争
go test -race ./...

# 构建时启用竞争检测
go build -race

# 运行时检测
go run -race main.go
```

**限制：**
- 执行速度慢 5-10 倍
- 内存使用多 5-10 倍
- 只能检测到实际执行的竞争
- 需要良好的测试覆盖率

### 最佳实践

1. **CI 中总是运行 `go test -race`**
2. **用互斥锁或原子操作保护共享状态**
3. **check-then-act 操作要持锁完成**
4. **文档说明每个锁保护什么**
5. **尽可能使用不可变数据**
6. **明确所有权语义**

