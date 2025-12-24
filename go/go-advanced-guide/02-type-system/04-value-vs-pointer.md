# Value vs Pointer Semantics

## 1. Engineering Problem

### What real-world problem does this solve?

**Understanding when Go copies data vs shares references is critical for correctness and performance.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    VALUE vs POINTER SEMANTICS                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Value Semantics:                  Pointer Semantics:                  │
│   ────────────────                  ──────────────────                  │
│                                                                         │
│   r := Route{Prefix: "10.0.0.0"}    r := &Route{Prefix: "10.0.0.0"}    │
│   s := r   // COPY                  s := r   // SAME pointer            │
│                                                                         │
│   ┌───────┐    ┌───────┐            ┌───────┐                           │
│   │   r   │    │   s   │            │   r   │────┐                      │
│   │ 10... │    │ 10... │            │ ptr   │    │    ┌───────┐         │
│   └───────┘    └───────┘            └───────┘    ├───►│ Route │         │
│                                     ┌───────┐    │    │ 10... │         │
│   s.Prefix = "192..."               │   s   │────┘    └───────┘         │
│   // r.Prefix unchanged             │ ptr   │                           │
│                                     └───────┘                           │
│                                     s.Prefix = "192..."                 │
│                                     // r.Prefix ALSO changes!           │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Built-in types and their semantics:                                   │
│                                                                         │
│   VALUE types:            REFERENCE types (contain pointers internally):│
│   • int, float, bool      • slice (header: ptr, len, cap)              │
│   • string (immutable)    • map (pointer to runtime struct)            │
│   • array                 • channel (pointer to runtime struct)         │
│   • struct                • interface (type, value pointers)            │
│                           • function (pointer to code)                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### When to use each

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DECISION GUIDE                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Use VALUE semantics when:                                             │
│   ─────────────────────────                                             │
│   • Type is small (< 64 bytes typically)                                │
│   • Type is immutable by design                                         │
│   • You want isolation (changes don't affect original)                  │
│   • Type represents a "value" concept (time.Time, net.IP)               │
│                                                                         │
│   Use POINTER semantics when:                                           │
│   ───────────────────────────                                           │
│   • Type is large (copying would be expensive)                          │
│   • Type is mutable (methods modify state)                              │
│   • Type has internal sync primitives (Mutex)                           │
│   • Type has identity (database connection, file handle)                │
│   • Any method uses pointer receiver → all should                       │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Standard library examples:                                            │
│                                                                         │
│   Value types:              Pointer types:                              │
│   • time.Time               • bytes.Buffer                              │
│   • net.IP                  • os.File                                   │
│   • url.URL                 • http.Client                               │
│   • context.Context         • sync.Mutex                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Passing to functions

```go
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// Pass by value - receives copy
func PrintRoute(r Route) {
    fmt.Printf("Route: %+v\n", r)
    r.Prefix = "modified"  // Only modifies the copy
}

// Pass by pointer - receives address
func UpdateRoute(r *Route) {
    r.Prefix = "modified"  // Modifies the original
}

func main() {
    r := Route{VrfID: 1, Prefix: "10.0.0.0/24"}
    
    PrintRoute(r)
    fmt.Println(r.Prefix)  // Still "10.0.0.0/24"
    
    UpdateRoute(&r)
    fmt.Println(r.Prefix)  // Now "modified"
}
```

### Slice internals

```go
// Slice is a struct: { ptr *T, len int, cap int }
// Passing slice passes the HEADER by value
// But header contains pointer to underlying array

func modifySlice(s []int) {
    s[0] = 100  // Modifies original array!
    s = append(s, 200)  // Does NOT affect original slice header
}

func main() {
    original := []int{1, 2, 3}
    modifySlice(original)
    fmt.Println(original)  // [100 2 3] - element modified
                           // But len is still 3 (append was local)
}

// To modify slice itself, pass pointer
func reallyModifySlice(s *[]int) {
    *s = append(*s, 200)
}
```

### Map internals

```go
// Map is a pointer to runtime.hmap
// Passing map passes the pointer (by value)

func modifyMap(m map[string]int) {
    m["key"] = 100  // Modifies original map
}

func main() {
    m := map[string]int{"a": 1}
    modifyMap(m)
    fmt.Println(m["key"])  // 100
}
```

---

## 4. Idiomatic Usage

### Method receivers: consistency is key

```go
// If ANY method needs pointer receiver, ALL should use pointer

type Route struct {
    mu      sync.Mutex  // Has mutex → must use pointer
    VrfID   uint32
    Prefix  string
    NextHop string
}

// All methods use pointer receiver
func (r *Route) Lock()                { r.mu.Lock() }
func (r *Route) Unlock()              { r.mu.Unlock() }
func (r *Route) SetNextHop(nh string) { r.NextHop = nh }
func (r *Route) GetPrefix() string    { return r.Prefix }  // Even getters!

// WHY? Consistency prevents confusion about whether
// you're working with a copy or the original
```

### Value types by design

```go
// time.Time is designed as value type
type Event struct {
    Name      string
    Timestamp time.Time  // Not *time.Time
}

// Methods use value receiver
func (t Time) Add(d Duration) Time {
    return Time{...}  // Returns new value, doesn't modify
}

// Copying is safe and expected
t1 := time.Now()
t2 := t1  // Independent copy
```

### When to return pointer vs value

```go
// Return pointer when:
// 1. Struct is large
// 2. Caller might modify
// 3. Struct has identity
func NewRouteManager() *RouteManager {
    return &RouteManager{
        routes: make(map[string]Route),
    }
}

// Return value when:
// 1. Struct is small
// 2. It's truly a value (time, coordinates)
// 3. You want to ensure immutability
func ParseRoute(s string) (Route, error) {
    // ...
    return Route{...}, nil
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Accidental copy with range

```go
type Route struct {
    VrfID  uint32
    Prefix string
    count  int
}

routes := []Route{{VrfID: 1}, {VrfID: 2}}

// BUG: v is a COPY
for _, v := range routes {
    v.count++  // Modifies copy, not original!
}
// routes[0].count is still 0

// FIX: Use index
for i := range routes {
    routes[i].count++
}

// Or use pointers
routePtrs := []*Route{{VrfID: 1}, {VrfID: 2}}
for _, r := range routePtrs {
    r.count++  // Works - modifies original
}
```

### Pitfall 2: Copying sync types

```go
type Counter struct {
    mu    sync.Mutex
    value int
}

c1 := Counter{}
c2 := c1  // BUG: Copies the mutex!

// c1 and c2 have independent mutexes now
// This breaks synchronization

// FIX: Always use pointers for types with sync primitives
c1 := &Counter{}
c2 := c1  // Same Counter
```

### Pitfall 3: nil pointer receiver

```go
type Logger struct {
    prefix string
}

func (l *Logger) Log(msg string) {
    // BUG: What if l is nil?
    fmt.Println(l.prefix + ": " + msg)  // Panic!
}

var l *Logger
l.Log("test")  // Method call is valid, but will panic

// FIX: Check for nil
func (l *Logger) Log(msg string) {
    if l == nil {
        return  // Or use default behavior
    }
    fmt.Println(l.prefix + ": " + msg)
}
```

### Pitfall 4: Interface holds pointer to temp value

```go
type Printer interface {
    Print()
}

type Route struct {
    Prefix string
}

func (r *Route) Print() {
    fmt.Println(r.Prefix)
}

func getPrinter() Printer {
    r := Route{Prefix: "10.0.0.0/24"}
    return &r  // Returns pointer to stack variable
}  // r goes out of scope, but pointer escapes to heap (OK in Go)

// Go handles this correctly via escape analysis
// But be aware that the object will live on heap
```

---

## 6. Complete Example

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// Value type - small, immutable concept
type IPAddress struct {
    bytes [4]byte  // Fixed size, small
}

func (ip IPAddress) String() string {
    return fmt.Sprintf("%d.%d.%d.%d", ip.bytes[0], ip.bytes[1], ip.bytes[2], ip.bytes[3])
}

func ParseIP(s string) IPAddress {
    var ip IPAddress
    fmt.Sscanf(s, "%d.%d.%d.%d", &ip.bytes[0], &ip.bytes[1], &ip.bytes[2], &ip.bytes[3])
    return ip  // Return by value
}

// Pointer type - has mutex, identity, mutable
type RouteTable struct {
    mu      sync.RWMutex
    routes  map[string]Route
    updated time.Time
}

type Route struct {
    VrfID     uint32
    Prefix    string
    NextHop   IPAddress  // Value - embedded
    CreatedAt time.Time  // Value - embedded
}

func NewRouteTable() *RouteTable {
    return &RouteTable{
        routes: make(map[string]Route),
    }
}

func (rt *RouteTable) Add(r Route) {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    key := fmt.Sprintf("%d:%s", r.VrfID, r.Prefix)
    r.CreatedAt = time.Now()
    rt.routes[key] = r
    rt.updated = time.Now()
}

func (rt *RouteTable) Get(vrfID uint32, prefix string) (Route, bool) {
    rt.mu.RLock()
    defer rt.mu.RUnlock()
    
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    r, ok := rt.routes[key]
    return r, ok  // Return copy of Route
}

// Demonstrating proper iteration
func (rt *RouteTable) UpdateAllNextHops(newHop IPAddress) {
    rt.mu.Lock()
    defer rt.mu.Unlock()
    
    // Must use key to modify map values
    for key, route := range rt.routes {
        route.NextHop = newHop
        rt.routes[key] = route  // Reassign modified copy
    }
}

// Demonstrating slice semantics
type RouteList struct {
    routes []Route
}

func (rl *RouteList) Add(r Route) {
    rl.routes = append(rl.routes, r)  // Must use pointer receiver
}

// Value receiver would fail:
// func (rl RouteList) Add(r Route) {
//     rl.routes = append(rl.routes, r)  // Lost!
// }

func (rl RouteList) Get(index int) Route {
    return rl.routes[index]  // Return copy
}

func (rl *RouteList) Modify(index int, newHop IPAddress) {
    rl.routes[index].NextHop = newHop  // Direct modification
}

// Function demonstrating parameter semantics
func processRoute(r Route) {
    // r is a copy - modifications don't affect caller
    r.Prefix = "modified"
}

func updateRoute(r *Route) {
    // r points to caller's Route - modifications persist
    r.Prefix = "modified"
}

func processRoutes(routes []Route) {
    // routes header is copy, but underlying array is shared
    routes[0].Prefix = "modified"  // DOES affect caller
    routes = append(routes, Route{})  // Does NOT affect caller's slice
}

func main() {
    // Value semantics with IPAddress
    ip1 := ParseIP("192.168.1.1")
    ip2 := ip1  // Copy
    fmt.Printf("ip1: %s, ip2: %s\n", ip1, ip2)
    
    // Pointer semantics with RouteTable
    rt := NewRouteTable()
    rt.Add(Route{
        VrfID:   1,
        Prefix:  "10.0.0.0/24",
        NextHop: ParseIP("192.168.1.1"),
    })
    rt.Add(Route{
        VrfID:   1,
        Prefix:  "10.0.1.0/24",
        NextHop: ParseIP("192.168.1.2"),
    })
    
    // Get returns copy
    r, _ := rt.Get(1, "10.0.0.0/24")
    r.Prefix = "modified"  // Doesn't affect table
    
    r2, _ := rt.Get(1, "10.0.0.0/24")
    fmt.Printf("Still original: %s\n", r2.Prefix)
    
    // Update all
    rt.UpdateAllNextHops(ParseIP("10.10.10.1"))
    
    // List with pointer receiver for Add
    list := &RouteList{}
    list.Add(Route{VrfID: 1, Prefix: "172.16.0.0/16"})
    list.Add(Route{VrfID: 2, Prefix: "192.168.0.0/16"})
    fmt.Printf("List has %d routes\n", len(list.routes))
    
    // Slice parameter semantics
    routes := []Route{{Prefix: "a"}, {Prefix: "b"}}
    processRoutes(routes)
    fmt.Printf("After processRoutes: %s\n", routes[0].Prefix)  // "modified"
    fmt.Printf("Length: %d\n", len(routes))  // Still 2
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    VALUE vs POINTER RULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   VALUE semantics:                                                      │
│   • Small structs (time.Time, net.IP)                                   │
│   • Immutable by design                                                 │
│   • No sync primitives                                                  │
│   • No identity                                                         │
│                                                                         │
│   POINTER semantics:                                                    │
│   • Large structs                                                       │
│   • Mutable state                                                       │
│   • Contains sync.Mutex, channels, etc.                                 │
│   • Has identity (connection, handle)                                   │
│                                                                         │
│   CONSISTENCY:                                                          │
│   • If ONE method needs pointer receiver, ALL should use it             │
│   • Don't mix value and pointer receivers on same type                  │
│                                                                         │
│   REMEMBER:                                                             │
│   • Slices, maps, channels contain internal pointers                    │
│   • range gives copies - use index to modify                            │
│   • Never copy sync types                                               │
│   • Escape analysis handles stack/heap placement                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 值语义 vs 指针语义

| 概念 | 值语义 | 指针语义 |
|------|--------|----------|
| 赋值 | 复制数据 | 复制地址（共享数据） |
| 传参 | 函数得到副本 | 函数得到原对象 |
| 修改 | 不影响原值 | 影响原对象 |

### 何时使用值语义

- 小结构体（< 64 字节）
- 不可变设计
- 无同步原语
- 表示"值"概念（时间、坐标）

### 何时使用指针语义

- 大结构体
- 可变状态
- 包含 sync.Mutex
- 有身份标识（数据库连接、文件句柄）

### 内置类型语义

| 类型 | 语义 | 说明 |
|------|------|------|
| int, float, bool | 值 | 完全复制 |
| string | 值 | 不可变，共享底层 |
| array | 值 | 完全复制 |
| struct | 值 | 完全复制 |
| slice | 引用 | 头部复制，底层数组共享 |
| map | 引用 | 是指向 runtime 结构的指针 |
| channel | 引用 | 是指向 runtime 结构的指针 |

### 常见陷阱

1. **range 循环复制**：`for _, v := range` 中 v 是副本
2. **复制 sync 类型**：Mutex 被复制后独立，破坏同步
3. **方法接收器不一致**：混用值和指针接收器

### 最佳实践

- 保持一致：如果任何方法用指针接收器，所有方法都用
- 小类型用值语义
- 有 Mutex 的类型必须用指针
- 修改 slice/map 元素用索引

