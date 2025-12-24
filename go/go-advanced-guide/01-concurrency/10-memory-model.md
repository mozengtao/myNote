# Go Memory Model: What You Must Know

## 1. Engineering Problem

### What real-world problem does this solve?

**The Go memory model defines when reads in one goroutine are guaranteed to see writes from another.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE VISIBILITY PROBLEM                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Modern CPUs and compilers reorder operations for performance:         │
│                                                                         │
│   What you write:          What might execute:                          │
│   ───────────────          ───────────────────                          │
│   a = 1                    b = 2                                        │
│   b = 2                    a = 1  (reordered!)                          │
│                                                                         │
│   Each CPU core has its own cache:                                      │
│                                                                         │
│   ┌────────────┐    ┌────────────┐                                      │
│   │   Core 0   │    │   Core 1   │                                      │
│   │ ┌────────┐ │    │ ┌────────┐ │                                      │
│   │ │ Cache  │ │    │ │ Cache  │ │  ◄── May have different values!     │
│   │ │ a = 1  │ │    │ │ a = 0  │ │                                      │
│   │ └────────┘ │    │ └────────┘ │                                      │
│   └────────────┘    └────────────┘                                      │
│          │                │                                             │
│          └────────┬───────┘                                             │
│                   ▼                                                     │
│            ┌────────────┐                                               │
│            │   Memory   │                                               │
│            │   a = ?    │                                               │
│            └────────────┘                                               │
│                                                                         │
│   Without synchronization, Goroutine B might not see Goroutine A's     │
│   writes - or might see them in a different order.                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Why does this matter?

```go
// This code is BROKEN without synchronization
var a, b int

go func() {
    a = 1
    b = 2
}()

go func() {
    if b == 2 {
        print(a)  // Might print 0! Even though b == 2
    }
}()
```

---

## 2. Core Mental Model

### The happens-before partial order

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    HAPPENS-BEFORE RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   If A happens-before B, then:                                          │
│   • A's writes are visible to B's reads                                 │
│   • A completes before B starts (logically, not necessarily in time)    │
│                                                                         │
│   Key synchronization points that establish happens-before:             │
│                                                                         │
│   1. SINGLE GOROUTINE                                                   │
│      Within one goroutine, statements execute in program order          │
│      a = 1 happens-before b = 2 (in same goroutine)                    │
│                                                                         │
│   2. GOROUTINE CREATION                                                 │
│      go f() happens-before f() starts executing                         │
│                                                                         │
│   3. CHANNEL SEND/RECEIVE                                               │
│      • Send happens-before receive completes                            │
│      • Close happens-before receive of zero value                       │
│      • Unbuffered: receive happens-before send completes                │
│                                                                         │
│   4. LOCKS                                                              │
│      Unlock happens-before subsequent Lock                              │
│                                                                         │
│   5. ONCE                                                               │
│      once.Do(f) completion happens-before any once.Do returns           │
│                                                                         │
│   6. ATOMIC                                                             │
│      Atomic operations with proper pairs (Store/Load) synchronize       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Synchronization Guarantees

### Channel synchronization

```go
// Unbuffered channel: synchronous
var a int

func main() {
    ch := make(chan struct{})
    
    go func() {
        a = 1
        ch <- struct{}{}  // Send
    }()
    
    <-ch  // Receive completes after send
    print(a)  // Guaranteed to print 1
}

// Buffered channel: send happens-before receive completes
func main() {
    ch := make(chan int, 1)
    
    go func() {
        a = 1
        ch <- 1  // Send happens-before...
    }()
    
    <-ch  // ...receive completes
    print(a)  // Guaranteed to print 1
}
```

### Mutex synchronization

```go
var a int
var mu sync.Mutex

func main() {
    go func() {
        mu.Lock()
        a = 1
        mu.Unlock()  // Unlock happens-before...
    }()
    
    mu.Lock()  // ...this Lock
    print(a)   // Guaranteed to print 1
    mu.Unlock()
}
```

### sync.WaitGroup synchronization

```go
var a int
var wg sync.WaitGroup

func main() {
    wg.Add(1)
    
    go func() {
        a = 1
        wg.Done()  // Done happens-before...
    }()
    
    wg.Wait()  // ...Wait returns
    print(a)   // Guaranteed to print 1
}
```

### sync.Once synchronization

```go
var a int
var once sync.Once

func setup() {
    a = 1
}

func main() {
    go func() {
        once.Do(setup)
    }()
    
    once.Do(setup)  // If this returns, setup has completed
    print(a)        // Guaranteed to print 1
}
```

### sync/atomic synchronization (Go 1.19+)

```go
var data int
var ready atomic.Bool

func producer() {
    data = 42
    ready.Store(true)  // Synchronizing store
}

func consumer() {
    for !ready.Load() {  // Synchronizing load
        time.Sleep(time.Millisecond)
    }
    print(data)  // Guaranteed to print 42
}
```

---

## 4. Common Mistakes

### Mistake 1: Assuming visibility without sync

```go
// BROKEN: No happens-before relationship
var done bool
var result string

go func() {
    result = "hello"
    done = true
}()

for !done {
    // Busy wait
}
print(result)  // May print "" or "hello" - undefined!

// FIX: Use channel or atomic
var done atomic.Bool
var result string

go func() {
    result = "hello"
    done.Store(true)  // Synchronizing store
}()

for !done.Load() {  // Synchronizing load
    time.Sleep(time.Millisecond)
}
print(result)  // Guaranteed to print "hello"
```

### Mistake 2: Double-checked locking without atomics

```go
// BROKEN: Classic double-checked locking bug
var instance *Singleton
var mu sync.Mutex

func GetInstance() *Singleton {
    if instance == nil {  // First check (unsynchronized read!)
        mu.Lock()
        if instance == nil {
            instance = newSingleton()  // Another goroutine might see partial init
        }
        mu.Unlock()
    }
    return instance
}

// FIX 1: Use sync.Once (recommended)
var (
    instance *Singleton
    once     sync.Once
)

func GetInstance() *Singleton {
    once.Do(func() {
        instance = newSingleton()
    })
    return instance
}

// FIX 2: Use atomic.Pointer (if you need the pattern)
var instance atomic.Pointer[Singleton]
var mu sync.Mutex

func GetInstance() *Singleton {
    if p := instance.Load(); p != nil {
        return p
    }
    
    mu.Lock()
    defer mu.Unlock()
    
    if p := instance.Load(); p != nil {
        return p
    }
    
    p := newSingleton()
    instance.Store(p)
    return p
}
```

### Mistake 3: Relying on statement order across goroutines

```go
// BROKEN: No guarantee about order
var a, b string

go func() {
    a = "hello"  // These could be reordered
    b = "world"  // by compiler or CPU
}()

go func() {
    if b == "world" {
        print(a)  // Might print ""!
    }
}()

// The CPU/compiler might reorder b = "world" before a = "hello"
// Or the second goroutine's cache might not have a's new value

// FIX: Use proper synchronization
var a, b atomic.Value

go func() {
    a.Store("hello")
    b.Store("world")
}()

go func() {
    for b.Load() != "world" {
        runtime.Gosched()
    }
    print(a.Load())  // Now guaranteed "hello"
}()
```

---

## 5. Practical Guidelines

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MEMORY MODEL GUIDELINES                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   DO:                                                                   │
│   ───                                                                   │
│   1. Use channels for communication between goroutines                  │
│   2. Protect shared state with sync.Mutex or sync.RWMutex              │
│   3. Use sync.Once for one-time initialization                          │
│   4. Use sync/atomic for simple flags and counters                      │
│   5. Think in terms of happens-before, not time                         │
│                                                                         │
│   DON'T:                                                                │
│   ──────                                                                │
│   1. Assume writes in one goroutine are visible in another              │
│   2. Use busy loops without synchronization                             │
│   3. Use unsafe package unless absolutely necessary                     │
│   4. Assume atomic operations provide ordering (before Go 1.19)         │
│   5. Try to be clever with lock-free algorithms                         │
│                                                                         │
│   WHEN IN DOUBT:                                                        │
│   ──────────────                                                        │
│   • Use channels                                                        │
│   • Use mutex                                                           │
│   • Run with -race                                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
    "time"
)

// SafeConfig demonstrates proper initialization pattern
type SafeConfig struct {
    once   sync.Once
    values map[string]string
}

func (c *SafeConfig) Get(key string) string {
    c.once.Do(func() {
        // This initialization is guaranteed to complete
        // before any Get() returns
        c.values = map[string]string{
            "host": "localhost",
            "port": "8080",
        }
        time.Sleep(10 * time.Millisecond) // Simulate slow init
    })
    return c.values[key]
}

// MessageQueue demonstrates channel-based synchronization
type MessageQueue struct {
    messages chan string
    done     chan struct{}
}

func NewMessageQueue(size int) *MessageQueue {
    return &MessageQueue{
        messages: make(chan string, size),
        done:     make(chan struct{}),
    }
}

func (q *MessageQueue) Send(msg string) {
    select {
    case q.messages <- msg:
    case <-q.done:
    }
}

func (q *MessageQueue) Receive() (string, bool) {
    select {
    case msg := <-q.messages:
        return msg, true
    case <-q.done:
        return "", false
    }
}

func (q *MessageQueue) Close() {
    close(q.done)
}

// Counter demonstrates atomic synchronization
type Counter struct {
    value atomic.Int64
    
    // Accompanying data synchronized by the atomic
    mu        sync.Mutex
    lastIncBy int64
}

func (c *Counter) Increment(by int64) int64 {
    c.mu.Lock()
    c.lastIncBy = by
    c.mu.Unlock()
    
    return c.value.Add(by)
}

func (c *Counter) Value() int64 {
    return c.value.Load()
}

// State demonstrates proper state management with mutex
type State struct {
    mu       sync.RWMutex
    data     map[string]int
    version  int64
}

func NewState() *State {
    return &State{
        data: make(map[string]int),
    }
}

func (s *State) Set(key string, value int) {
    s.mu.Lock()
    defer s.mu.Unlock()
    
    s.data[key] = value
    s.version++
}

func (s *State) Get(key string) (int, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    v, ok := s.data[key]
    return v, ok
}

func (s *State) Snapshot() map[string]int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    // Return a copy - safe for concurrent use
    result := make(map[string]int, len(s.data))
    for k, v := range s.data {
        result[k] = v
    }
    return result
}

// Demonstrate all patterns
func main() {
    fmt.Println("=== Go Memory Model Demo ===")
    
    // sync.Once pattern
    fmt.Println("\n--- sync.Once ---")
    config := &SafeConfig{}
    
    var wg sync.WaitGroup
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            host := config.Get("host")
            fmt.Printf("Goroutine %d: host = %s\n", id, host)
        }(i)
    }
    wg.Wait()
    
    // Channel pattern
    fmt.Println("\n--- Channels ---")
    queue := NewMessageQueue(10)
    
    wg.Add(1)
    go func() {
        defer wg.Done()
        for i := 0; i < 5; i++ {
            queue.Send(fmt.Sprintf("message-%d", i))
        }
    }()
    
    wg.Add(1)
    go func() {
        defer wg.Done()
        time.Sleep(10 * time.Millisecond)
        for {
            msg, ok := queue.Receive()
            if !ok {
                break
            }
            fmt.Printf("Received: %s\n", msg)
        }
    }()
    
    time.Sleep(100 * time.Millisecond)
    queue.Close()
    wg.Wait()
    
    // Atomic pattern
    fmt.Println("\n--- Atomic ---")
    counter := &Counter{}
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            counter.Increment(int64(id))
        }(i)
    }
    wg.Wait()
    fmt.Printf("Counter value: %d (expected: 45)\n", counter.Value())
    
    // Mutex pattern
    fmt.Println("\n--- Mutex ---")
    state := NewState()
    
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", id)
            state.Set(key, id*10)
        }(i)
    }
    wg.Wait()
    
    snapshot := state.Snapshot()
    fmt.Printf("State snapshot: %v\n", snapshot)
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 7. Design Takeaways

### Memory Model Summary

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MEMORY MODEL CHEAT SHEET                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Synchronization primitive          Happens-before guarantee           │
│   ────────────────────────          ─────────────────────────           │
│   make(chan T) (unbuffered)         send → receive completes            │
│                                     receive → send completes            │
│                                                                         │
│   make(chan T, N) (buffered)        send → receive completes            │
│                                                                         │
│   close(ch)                         close → receive returns (0, false)  │
│                                                                         │
│   sync.Mutex                        Unlock → Lock                       │
│                                                                         │
│   sync.RWMutex                      Unlock → Lock                       │
│                                     RUnlock → Lock                      │
│                                                                         │
│   sync.WaitGroup                    Done → Wait returns                 │
│                                                                         │
│   sync.Once                         Do(f) completes → any Do returns    │
│                                                                         │
│   sync/atomic (Go 1.19+)            Store → Load of same variable       │
│                                                                         │
│   go f()                            statement before go → f() starts    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Go 内存模型核心概念

**Go 内存模型定义了一个 goroutine 中的读操作何时能保证看到另一个 goroutine 的写操作。**

### 为什么需要内存模型？

1. **CPU 缓存**：每个 CPU 核心有独立缓存，可能看到不同的值
2. **指令重排序**：编译器和 CPU 可能重新排列指令
3. **可见性问题**：一个 goroutine 的写可能对另一个不可见

### happens-before 关系

如果 A happens-before B，则：
- A 的写对 B 的读可见
- A 在 B 之前完成（逻辑上）

### 建立 happens-before 的方式

| 操作 | 保证 |
|------|------|
| 无缓冲 channel | 发送 → 接收完成；接收 → 发送完成 |
| 有缓冲 channel | 发送 → 接收完成 |
| close(ch) | 关闭 → 接收返回 (0, false) |
| sync.Mutex | Unlock → Lock |
| sync.WaitGroup | Done → Wait 返回 |
| sync.Once | Do(f) 完成 → 任何 Do 返回 |
| sync/atomic | Store → Load（同一变量） |
| go f() | go 之前的语句 → f() 开始 |

### 实践准则

**应该做：**
1. 使用 channel 在 goroutine 间通信
2. 使用 mutex 保护共享状态
3. 使用 sync.Once 进行一次性初始化
4. 使用 atomic 处理简单的标志和计数器

**不应该做：**
1. 假设写操作对其他 goroutine 自动可见
2. 使用忙等待而不用同步原语
3. 尝试实现无锁算法（除非你是专家）

**有疑问时：**
- 使用 channel
- 使用 mutex
- 运行 `-race`

