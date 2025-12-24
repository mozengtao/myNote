# Method Receivers: T vs *T

## 1. Engineering Problem

### What real-world problem does this solve?

**The receiver type determines whether methods work on copies or the original.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    METHOD RECEIVER COMPARISON                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Value Receiver (T):               Pointer Receiver (*T):              │
│   ──────────────────                ────────────────────                │
│                                                                         │
│   func (r Route) Print() {          func (r *Route) Update() {          │
│       fmt.Println(r.Prefix)             r.Prefix = "new"                │
│   }                                 }                                   │
│                                                                         │
│   • Works on COPY of r              • Works on ORIGINAL r               │
│   • Cannot modify original          • Can modify original               │
│   • Called on value or pointer      • Can be called on value or pointer │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Calling conventions:                                                  │
│                                                                         │
│   r := Route{}                      r := Route{}                        │
│   r.Print()     // OK               r.Update()    // OK - auto &r       │
│   (&r).Print()  // OK               (&r).Update() // OK                 │
│                                                                         │
│   rp := &Route{}                    rp := &Route{}                      │
│   rp.Print()    // OK - auto *rp    rp.Update()   // OK                 │
│   (*rp).Print() // OK               (*rp).Update()// OK                 │
│                                                                         │
│   Go automatically takes address or dereferences                        │
│   when calling methods                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### The decision framework

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    RECEIVER TYPE DECISION                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Use POINTER receiver (*T) when:                                       │
│   ───────────────────────────────                                       │
│   ✓ Method modifies the receiver                                        │
│   ✓ Type contains sync primitives (Mutex, etc.)                         │
│   ✓ Type is large (avoid copying)                                       │
│   ✓ ANY other method on type uses pointer receiver                      │
│   ✓ Type is typically used as pointer (*T)                              │
│                                                                         │
│   Use VALUE receiver (T) when:                                          │
│   ────────────────────────────                                          │
│   ✓ Type is small and immutable                                         │
│   ✓ Method only reads data                                              │
│   ✓ Type is similar to primitive (time.Time, net.IP)                    │
│   ✓ Copying is cheap and desirable                                      │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   GOLDEN RULE: Be consistent                                            │
│   • If any method needs *T, all should use *T                           │
│   • Mixing causes confusion about semantics                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Basic syntax

```go
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// Value receiver - receives copy
func (r Route) String() string {
    return fmt.Sprintf("%d:%s via %s", r.VrfID, r.Prefix, r.NextHop)
}

// Pointer receiver - receives pointer to original
func (r *Route) SetNextHop(nh string) {
    r.NextHop = nh
}
```

### Automatic address/dereference

```go
r := Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"}

// Go takes address automatically
r.SetNextHop("10.0.0.1")  // Equivalent to (&r).SetNextHop("10.0.0.1")

rp := &Route{VrfID: 2, Prefix: "172.16.0.0/16"}

// Go dereferences automatically
fmt.Println(rp.String())  // Equivalent to (*rp).String()
```

### Interface satisfaction

```go
type Stringer interface {
    String() string
}

type Modifier interface {
    Modify()
}

type Data struct{ value int }

func (d Data) String() string { return fmt.Sprintf("%d", d.value) }
func (d *Data) Modify()       { d.value++ }

var s Stringer
var m Modifier

d := Data{42}
dp := &Data{42}

s = d    // OK: Data has String()
s = dp   // OK: *Data also has String()

m = d    // ERROR: Data does not have Modify()
m = dp   // OK: *Data has Modify()

// Rule: Pointer receiver methods are NOT in the value's method set
```

---

## 4. Idiomatic Usage

### Pattern 1: Mutable types always use pointer

```go
// All methods use pointer - consistent and correct
type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func (rm *RouteManager) Add(r Route)                    { ... }
func (rm *RouteManager) Get(key string) (Route, bool)   { ... }
func (rm *RouteManager) Delete(key string)              { ... }
func (rm *RouteManager) Count() int                     { ... } // Even getters!
```

### Pattern 2: Immutable value types

```go
// time.Time uses value receivers - immutable
type Time struct { ... }

func (t Time) Add(d Duration) Time { ... }      // Returns new Time
func (t Time) Sub(u Time) Duration { ... }
func (t Time) Before(u Time) bool { ... }

// Never modifies t, always returns new value
```

### Pattern 3: Small read-only structs

```go
type Point struct {
    X, Y float64
}

func (p Point) Distance(q Point) float64 {
    dx := q.X - p.X
    dy := q.Y - p.Y
    return math.Sqrt(dx*dx + dy*dy)
}

// Small (16 bytes), read-only, value receiver is fine
```

---

## 5. Common Pitfalls

### Pitfall 1: Method doesn't modify original

```go
type Counter struct {
    value int
}

// BUG: Value receiver - modifies copy
func (c Counter) Increment() {
    c.value++  // Modifies the copy!
}

func main() {
    c := Counter{value: 0}
    c.Increment()
    fmt.Println(c.value)  // Still 0!
}

// FIX: Use pointer receiver
func (c *Counter) Increment() {
    c.value++
}
```

### Pitfall 2: Can't take address of map value

```go
type Counters map[string]Counter

func (cs Counters) Increment(key string) {
    // BUG: Can't take address of map value
    cs[key].Increment()  // Won't work if Increment has pointer receiver
    
    // FIX: Store pointers in map
    // OR: Get, modify, put back
    c := cs[key]
    c.Increment()  // If value receiver
    cs[key] = c
}

// Better: Map of pointers
type Counters map[string]*Counter

func (cs Counters) Increment(key string) {
    cs[key].Increment()  // Works with pointer receiver
}
```

### Pitfall 3: Interface satisfaction mismatch

```go
type Writer interface {
    Write([]byte) (int, error)
}

type MyWriter struct {
    data []byte
}

func (w *MyWriter) Write(b []byte) (int, error) {
    w.data = append(w.data, b...)
    return len(b), nil
}

func main() {
    var w Writer
    
    mw := MyWriter{}
    w = mw   // ERROR: MyWriter doesn't implement Writer
    w = &mw  // OK: *MyWriter implements Writer
}
```

### Pitfall 4: nil receiver

```go
type Logger struct {
    prefix string
}

func (l *Logger) Log(msg string) {
    fmt.Println(l.prefix + msg)  // Panics if l is nil!
}

var l *Logger  // nil
l.Log("test")  // Panic!

// FIX: Check for nil in method
func (l *Logger) Log(msg string) {
    if l == nil {
        fmt.Println(msg)  // Default behavior
        return
    }
    fmt.Println(l.prefix + msg)
}
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "sync"
)

// Immutable value type - all value receivers
type IPAddress struct {
    bytes [4]byte
}

func (ip IPAddress) String() string {
    return fmt.Sprintf("%d.%d.%d.%d",
        ip.bytes[0], ip.bytes[1], ip.bytes[2], ip.bytes[3])
}

func (ip IPAddress) IsPrivate() bool {
    return ip.bytes[0] == 10 ||
        (ip.bytes[0] == 172 && ip.bytes[1] >= 16 && ip.bytes[1] <= 31) ||
        (ip.bytes[0] == 192 && ip.bytes[1] == 168)
}

// Returns new value, doesn't modify
func (ip IPAddress) NextIP() IPAddress {
    result := ip
    result.bytes[3]++
    // Handle overflow...
    return result
}

// Mutable type with sync - all pointer receivers
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop IPAddress
}

type RouteTable struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewRouteTable() *RouteTable {
    return &RouteTable{
        routes: make(map[string]Route),
    }
}

// Modifies state - pointer receiver
func (rt *RouteTable) Add(r Route) {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    rt.routes[key] = r
}

// Reads state - still pointer receiver for consistency
func (rt *RouteTable) Get(vrfID uint32, prefix string) (Route, bool) {
    rt.mu.RLock()
    defer rt.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rt.routes[key]
    return r, ok
}

// Reads state - pointer receiver
func (rt *RouteTable) Count() int {
    rt.mu.RLock()
    defer rt.mu.RUnlock()
    return len(rt.routes)
}

// Modifies state - pointer receiver
func (rt *RouteTable) Delete(vrfID uint32, prefix string) bool {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    if _, ok := rt.routes[key]; ok {
        delete(rt.routes, key)
        return true
    }
    return false
}

// Interface to demonstrate method set rules
type RouteStore interface {
    Add(Route)
    Get(vrfID uint32, prefix string) (Route, bool)
    Count() int
}

func PrintStoreInfo(store RouteStore) {
    fmt.Printf("Store has %d routes\n", store.Count())
}

func main() {
    // IPAddress value semantics
    ip := IPAddress{bytes: [4]byte{192, 168, 1, 1}}
    fmt.Printf("IP: %s, Private: %v\n", ip, ip.IsPrivate())
    
    nextIP := ip.NextIP()  // Returns new value
    fmt.Printf("Next: %s\n", nextIP)
    fmt.Printf("Original unchanged: %s\n", ip)
    
    // RouteTable pointer semantics
    rt := NewRouteTable()
    
    rt.Add(Route{
        VrfID:   1,
        Prefix:  "10.0.0.0/24",
        NextHop: IPAddress{bytes: [4]byte{192, 168, 1, 1}},
    })
    
    rt.Add(Route{
        VrfID:   1,
        Prefix:  "10.0.1.0/24",
        NextHop: IPAddress{bytes: [4]byte{192, 168, 1, 2}},
    })
    
    // Interface usage - must pass pointer
    PrintStoreInfo(rt)  // rt is *RouteTable, satisfies RouteStore
    
    // Cannot use:
    // rtValue := *rt
    // PrintStoreInfo(rtValue)  // ERROR: RouteTable doesn't implement RouteStore
    
    // Query
    if r, ok := rt.Get(1, "10.0.0.0/24"); ok {
        fmt.Printf("Found: %+v\n", r)
    }
    
    fmt.Printf("Total routes: %d\n", rt.Count())
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    METHOD RECEIVER RULES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. BE CONSISTENT                                                      │
│      • If ANY method needs *T, ALL should use *T                        │
│      • Don't mix receivers on the same type                             │
│                                                                         │
│   2. USE POINTER (*T) WHEN:                                             │
│      • Method modifies receiver                                         │
│      • Type has sync primitives                                         │
│      • Type is large                                                    │
│      • Other methods use pointer                                        │
│                                                                         │
│   3. USE VALUE (T) WHEN:                                                │
│      • Type is small and immutable                                      │
│      • Type is like a primitive                                         │
│      • All methods are read-only                                        │
│                                                                         │
│   4. INTERFACE RULES:                                                   │
│      • T methods are in T and *T method sets                            │
│      • *T methods are ONLY in *T method set                             │
│                                                                         │
│   5. NIL RECEIVERS:                                                     │
│      • *T receiver can be nil                                           │
│      • Handle nil explicitly or document requirement                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 方法接收器选择

| 使用指针接收器 (*T) | 使用值接收器 (T) |
|---------------------|------------------|
| 方法修改接收器 | 类型小且不可变 |
| 类型包含同步原语 | 类似原始类型 |
| 类型很大 | 所有方法只读 |
| 其他方法用指针 | 复制是廉价的 |

### 关键规则

1. **保持一致**：如果任何方法用 *T，所有方法都用 *T
2. **接口满足**：*T 的方法不在 T 的方法集中
3. **自动转换**：Go 自动取地址或解引用来调用方法

### 接口方法集

```go
type T struct{}
func (t T) ValueMethod() {}
func (t *T) PointerMethod() {}

// T 的方法集：ValueMethod
// *T 的方法集：ValueMethod, PointerMethod

var i interface{ PointerMethod() }
i = T{}   // 错误！T 没有 PointerMethod
i = &T{}  // 正确
```

### 常见陷阱

1. **值接收器不能修改原值**
2. **不能取 map 值的地址**
3. **接口满足规则不匹配**
4. **nil 接收器可能 panic**

