# Zero Values and Zero-Cost Initialization

## 1. Engineering Problem

### What real-world problem does this solve?

**Go guarantees that all variables have a defined value - no uninitialized memory.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ZERO VALUE GUARANTEE                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   C/C++ (undefined behavior risk):                                      │
│   ────────────────────────────────                                      │
│                                                                         │
│   int x;        // Undefined! Could be anything                         │
│   char* s;      // Undefined! Dangerous pointer                         │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Go (guaranteed zero values):                                          │
│   ────────────────────────────                                          │
│                                                                         │
│   var x int       // 0                                                  │
│   var s string    // "" (empty string)                                  │
│   var p *int      // nil                                                │
│   var m map[k]v   // nil (but can check and initialize)                 │
│   var sl []int    // nil (len=0, cap=0)                                 │
│   var b bool      // false                                              │
│   var f float64   // 0.0                                                │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Benefits:                                                             │
│   • No uninitialized memory bugs                                        │
│   • Simpler reasoning about program state                               │
│   • Some zero values are immediately useful                             │
│   • Enables "make the zero value useful" design pattern                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Zero values by type

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ZERO VALUE TABLE                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Type                    │ Zero Value                                  │
│   ────────────────────────┼─────────────────────────────────────────    │
│   bool                    │ false                                       │
│   int, uint, float, etc   │ 0                                           │
│   string                  │ "" (empty string)                           │
│   pointer (*T)            │ nil                                         │
│   slice ([]T)             │ nil (len=0, cap=0)                          │
│   map (map[K]V)           │ nil                                         │
│   channel (chan T)        │ nil                                         │
│   interface               │ nil                                         │
│   function                │ nil                                         │
│   struct                  │ all fields are their zero values            │
│   array ([N]T)            │ all elements are their zero values          │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Usable zero values:              Unusable zero values:                │
│   ───────────────────              ────────────────────                 │
│   • slice (append works)           • map (write panics)                 │
│   • bytes.Buffer                   • channel (blocks forever)           │
│   • sync.Mutex                     • *T (nil pointer)                   │
│   • sync.WaitGroup                                                      │
│   • sync.Once                                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Declaration and zero values

```go
// Variable declarations - all get zero values
var x int         // 0
var s string      // ""
var b bool        // false
var p *Route      // nil
var m map[string]int  // nil - cannot write!
var sl []int      // nil - but append works!

// Struct with zero values
var r Route  // {VrfID: 0, Prefix: "", NextHop: "", IsV6: false}

// Short declaration with explicit value
x := 0  // Same as var x int
```

### nil slice vs empty slice

```go
var nilSlice []int          // nil, len=0, cap=0
emptySlice := []int{}       // not nil, len=0, cap=0
makeSlice := make([]int, 0) // not nil, len=0, cap=0

// Functionally equivalent for most operations
len(nilSlice) == 0  // true
append(nilSlice, 1) // works!

// But different for JSON
json.Marshal(nilSlice)  // null
json.Marshal(emptySlice) // []
```

### nil map vs empty map

```go
var nilMap map[string]int    // nil - CANNOT write
emptyMap := map[string]int{} // not nil - can write

nilMap["key"] = 1  // PANIC!
emptyMap["key"] = 1 // OK

// Reading from nil map is safe
v := nilMap["key"]  // Returns zero value (0)
```

---

## 4. Idiomatic Usage

### Design zero values to be useful

```go
// Good: Zero value is usable
type Counter struct {
    mu    sync.Mutex  // Zero value works
    count int         // Starts at 0
}

func (c *Counter) Inc() {
    c.mu.Lock()        // Works without initialization
    defer c.mu.Unlock()
    c.count++
}

// Usage - no constructor needed
var c Counter
c.Inc()  // Just works!
```

### bytes.Buffer pattern

```go
// bytes.Buffer's zero value is an empty, usable buffer
var buf bytes.Buffer  // Ready to use!

buf.WriteString("hello")
buf.WriteString(" world")
fmt.Println(buf.String())  // "hello world"
```

### Lazy initialization

```go
type Config struct {
    once   sync.Once
    values map[string]string
}

func (c *Config) Get(key string) string {
    c.once.Do(func() {
        c.values = loadConfig()
    })
    return c.values[key]
}

// Usage
var cfg Config  // Zero value
cfg.Get("key")  // Initializes on first use
```

### Constructor when zero value is unusable

```go
// Zero value is not useful - needs constructor
type Server struct {
    handler http.Handler  // nil is not useful
    clients map[string]*Client
    done    chan struct{}
}

// Constructor initializes required fields
func NewServer(h http.Handler) *Server {
    return &Server{
        handler: h,
        clients: make(map[string]*Client),
        done:    make(chan struct{}),
    }
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Writing to nil map

```go
type Cache struct {
    items map[string]string  // Zero value is nil
}

func (c *Cache) Set(k, v string) {
    c.items[k] = v  // PANIC if items is nil!
}

// FIX: Check and initialize
func (c *Cache) Set(k, v string) {
    if c.items == nil {
        c.items = make(map[string]string)
    }
    c.items[k] = v
}

// BETTER: Use constructor
func NewCache() *Cache {
    return &Cache{
        items: make(map[string]string),
    }
}
```

### Pitfall 2: Sending on nil channel

```go
type Worker struct {
    jobs chan Job  // Zero value is nil
}

func (w *Worker) Submit(j Job) {
    w.jobs <- j  // BLOCKS FOREVER on nil channel!
}

// FIX: Always initialize channels
func NewWorker() *Worker {
    return &Worker{
        jobs: make(chan Job),
    }
}
```

### Pitfall 3: Comparing with nil incorrectly

```go
type Result struct {
    Data  []byte
    Error error
}

// BUG: slice nil check doesn't work as expected
func (r Result) IsEmpty() bool {
    return r.Data == nil  // Doesn't distinguish nil from empty
}

// BETTER: Check length
func (r Result) IsEmpty() bool {
    return len(r.Data) == 0
}
```

### Pitfall 4: Zero value time.Time

```go
type Event struct {
    Timestamp time.Time
}

// Zero value is "0001-01-01 00:00:00 +0000 UTC"
var e Event
fmt.Println(e.Timestamp.IsZero())  // true

// Check for unset time
if e.Timestamp.IsZero() {
    e.Timestamp = time.Now()
}
```

---

## 6. Complete Example

```go
package main

import (
    "bytes"
    "fmt"
    "sync"
    "time"
)

// Counter - zero value is useful
type Counter struct {
    mu    sync.Mutex  // Works at zero value
    value int         // Starts at 0
}

func (c *Counter) Inc() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.value++
    return c.value
}

func (c *Counter) Value() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.value
}

// Route - zero value is mostly useful
type Route struct {
    VrfID     uint32
    Prefix    string
    NextHop   string
    CreatedAt time.Time  // Zero means unset
}

func (r Route) IsValid() bool {
    return r.Prefix != ""  // Check against zero value
}

func (r Route) IsNew() bool {
    return r.CreatedAt.IsZero()  // time.Time zero check
}

// RouteManager - zero value is NOT useful
type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route  // nil is not useful
    
    // Lazy init pattern
    initOnce sync.Once
}

func (rm *RouteManager) init() {
    rm.initOnce.Do(func() {
        rm.routes = make(map[string]Route)
    })
}

func (rm *RouteManager) Add(r Route) {
    rm.init()  // Ensure initialized
    
    rm.mu.Lock()
    defer rm.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    if r.CreatedAt.IsZero() {
        r.CreatedAt = time.Now()
    }
    rm.routes[key] = r
}

func (rm *RouteManager) Get(vrfID uint32, prefix string) (Route, bool) {
    rm.init()
    
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rm.routes[key]
    return r, ok
}

// Logger - zero value uses defaults
type Logger struct {
    prefix string
    mu     sync.Mutex
    buf    bytes.Buffer  // Zero value is usable!
}

func (l *Logger) Log(msg string) {
    l.mu.Lock()
    defer l.mu.Unlock()
    
    prefix := l.prefix
    if prefix == "" {
        prefix = "[LOG]"  // Default for zero value
    }
    
    fmt.Fprintf(&l.buf, "%s %s\n", prefix, msg)
}

func (l *Logger) String() string {
    l.mu.Lock()
    defer l.mu.Unlock()
    return l.buf.String()
}

// SliceCollector - nil slice is useful
type SliceCollector struct {
    items []string  // nil is fine
}

func (c *SliceCollector) Add(item string) {
    // nil slice handles append correctly
    c.items = append(c.items, item)
}

func (c *SliceCollector) Items() []string {
    return c.items  // Might be nil, but that's OK
}

func main() {
    // Counter - zero value works
    var c Counter
    fmt.Println("Counter:", c.Inc(), c.Inc(), c.Inc())
    
    // Route - check for zero value
    var r Route
    fmt.Println("Route valid:", r.IsValid())  // false
    fmt.Println("Route new:", r.IsNew())      // true
    
    r = Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}
    fmt.Println("Route valid:", r.IsValid())  // true
    fmt.Println("Route new:", r.IsNew())      // true (no CreatedAt)
    
    // RouteManager - lazy init
    var rm RouteManager
    rm.Add(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    if route, ok := rm.Get(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", route)
    }
    
    // Logger - zero value with defaults
    var log Logger
    log.Log("message 1")
    log.Log("message 2")
    fmt.Print(log.String())
    
    // SliceCollector - nil slice is fine
    var collector SliceCollector
    collector.Add("item1")
    collector.Add("item2")
    fmt.Println("Items:", collector.Items())
    
    // bytes.Buffer - zero value is usable
    var buf bytes.Buffer
    buf.WriteString("hello")
    buf.WriteString(" world")
    fmt.Println("Buffer:", buf.String())
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ZERO VALUE DESIGN RULES                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. MAKE ZERO VALUE USEFUL                                             │
│      • sync.Mutex: ready to use                                         │
│      • bytes.Buffer: empty buffer ready                                 │
│      • Counter: starts at 0                                             │
│                                                                         │
│   2. USE LAZY INITIALIZATION FOR COMPLEX STATE                          │
│      • sync.Once for thread-safe lazy init                              │
│      • Check-and-create for maps                                        │
│                                                                         │
│   3. PROVIDE CONSTRUCTORS WHEN NEEDED                                   │
│      • Required fields (channels, handlers)                             │
│      • Complex initialization                                           │
│      • Validation needed                                                │
│                                                                         │
│   4. DOCUMENT ZERO VALUE BEHAVIOR                                       │
│      • If zero value is not useful, say so                              │
│      • If zero value has special meaning, document it                   │
│                                                                         │
│   5. USEFUL ZERO VALUES:                                                │
│      • slice (append works)                                             │
│      • sync types (Mutex, WaitGroup, Once)                              │
│      • bytes.Buffer, strings.Builder                                    │
│                                                                         │
│   6. UNUSABLE ZERO VALUES:                                              │
│      • map (write panics)                                               │
│      • channel (blocks forever)                                         │
│      • pointers (nil dereference)                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 零值保证

**Go 保证所有变量都有定义的初始值——没有未初始化的内存。**

### 各类型的零值

| 类型 | 零值 | 可直接使用 |
|------|------|-----------|
| bool | false | ✓ |
| int, float | 0 | ✓ |
| string | "" | ✓ |
| slice | nil | ✓（append 可用） |
| map | nil | ✗（写入 panic） |
| channel | nil | ✗（永远阻塞） |
| pointer | nil | ✗（解引用 panic） |
| struct | 所有字段为零值 | 取决于字段 |
| sync.Mutex | 可用状态 | ✓ |

### 设计原则

1. **让零值有用**：设计类型使零值是有效的初始状态
2. **延迟初始化**：对于复杂状态，用 sync.Once 延迟初始化
3. **需要时提供构造函数**：当零值无法使用时

### 常见陷阱

1. **写入 nil map**：panic
2. **向 nil channel 发送**：永远阻塞
3. **与 nil 比较**：slice 用 len 检查更好
4. **time.Time 零值**：使用 IsZero() 检查

### 最佳实践

- sync.Mutex、bytes.Buffer 的零值可直接使用
- slice 的零值可以 append
- map 必须初始化才能写入
- 文档说明零值行为

