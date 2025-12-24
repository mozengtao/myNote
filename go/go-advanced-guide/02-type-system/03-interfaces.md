# Interfaces: Implicit Implementation

## 1. Engineering Problem

### What real-world problem does this solve?

**Go interfaces enable polymorphism without class hierarchies.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    INTERFACE MODEL COMPARISON                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Explicit Interfaces (Java/C#):                                        │
│   ──────────────────────────────                                        │
│                                                                         │
│   class MyReader implements Reader {  // Must declare                   │
│       public int read(byte[] b) { ... }                                 │
│   }                                                                     │
│                                                                         │
│   Problems:                                                             │
│   • Must anticipate all interfaces at definition time                   │
│   • Can't add new interfaces to existing types                          │
│   • Leads to interface bloat                                            │
│   • Tight coupling between packages                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Implicit Interfaces (Go):                                             │
│   ─────────────────────────                                             │
│                                                                         │
│   type Reader interface {           type MyReader struct{}              │
│       Read([]byte) (int, error)     func (r MyReader) Read(b []byte)    │
│   }                                     (int, error) { ... }            │
│                                                                         │
│   // MyReader implements Reader automatically!                          │
│   // No "implements" keyword needed                                     │
│   var r Reader = MyReader{}  // Just works                              │
│                                                                         │
│   Benefits:                                                             │
│   • Define interface where it's USED, not where type is defined         │
│   • Retrofit interfaces onto existing types                             │
│   • Small, focused interfaces                                           │
│   • Decoupled packages                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### The "duck typing" philosophy

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    INTERFACE SATISFACTION                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   "If it walks like a duck and quacks like a duck, it's a duck"         │
│                                                                         │
│   interface Writer {                   Satisfies Writer?                │
│       Write([]byte) (int, error)       ─────────────────                │
│   }                                                                     │
│                                        ┌──────────────────┐             │
│                                        │ os.File         │ ✓ Yes       │
│                                        │ bytes.Buffer    │ ✓ Yes       │
│                                        │ net.Conn        │ ✓ Yes       │
│                                        │ http.Response   │ ✓ Yes       │
│                                        │ any custom type │ ✓ If has    │
│                                        │   with Write()  │   method    │
│                                        └──────────────────┘             │
│                                                                         │
│   Key insight: The CONSUMER defines the interface                       │
│                The PRODUCER just has methods                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Interface values are (type, value) pairs

```go
var w io.Writer  // nil interface: (nil, nil)

w = os.Stdout    // interface value: (*os.File, pointer to stdout)

// Interface value structure:
// ┌─────────────┐
// │    type     │ → *os.File type information
// ├─────────────┤
// │    value    │ → actual *os.File pointer
// └─────────────┘
```

---

## 3. Language Mechanism

### Defining interfaces

```go
// Single method interface (most common in Go)
type Reader interface {
    Read(p []byte) (n int, err error)
}

// Multiple methods
type ReadWriter interface {
    Read(p []byte) (n int, err error)
    Write(p []byte) (n int, err error)
}

// Embedding interfaces
type ReadWriteCloser interface {
    Reader        // Embed Reader
    Writer        // Embed Writer
    Close() error
}

// Empty interface (any type satisfies it)
type any interface{}  // Go 1.18+ has built-in 'any'
```

### Type assertions

```go
var w io.Writer = os.Stdout

// Type assertion: get concrete type
f, ok := w.(*os.File)
if ok {
    // f is *os.File
}

// Without ok - panics if wrong type
f := w.(*os.File)  // Panics if w is not *os.File
```

### Type switches

```go
func describe(i interface{}) {
    switch v := i.(type) {
    case int:
        fmt.Printf("Integer: %d\n", v)
    case string:
        fmt.Printf("String: %s\n", v)
    case io.Reader:
        fmt.Println("It's a Reader")
    default:
        fmt.Printf("Unknown type: %T\n", v)
    }
}
```

---

## 4. Idiomatic Usage

### Small interfaces

```go
// Go standard library examples:
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

type Stringer interface {
    String() string
}

type error interface {
    Error() string
}

// Most interfaces have 1-3 methods
// "The bigger the interface, the weaker the abstraction"
```

### Accept interfaces, return structs

```go
// Good: Accept interface
func ProcessRoutes(r RouteReader) error {
    // Can work with any RouteReader implementation
}

// Good: Return concrete type
func NewRouteManager() *RouteManager {
    return &RouteManager{}
}

// Bad: Return interface (usually)
func NewRouteManager() RouteReader {
    return &RouteManager{}  // Hides implementation details unnecessarily
}
```

### Consumer-defined interfaces

```go
// Package database (producer)
type UserStore struct { ... }
func (s *UserStore) GetUser(id string) (*User, error)
func (s *UserStore) SaveUser(u *User) error

// Package api (consumer) - defines its own interface
type UserGetter interface {
    GetUser(id string) (*User, error)
}

func NewHandler(users UserGetter) *Handler {
    // Handler only needs GetUser, doesn't care about SaveUser
    return &Handler{users: users}
}

// This allows easy testing and decoupling
```

---

## 5. Common Pitfalls

### Pitfall 1: nil interface vs nil concrete value

```go
type MyError struct{ msg string }
func (e *MyError) Error() string { return e.msg }

func returnsError() error {
    var e *MyError = nil
    return e  // Returns (*MyError, nil), NOT nil!
}

err := returnsError()
if err != nil {  // TRUE! err is not nil
    fmt.Println("Error:", err)  // Might crash on e.msg access
}

// Fix: Return nil explicitly
func returnsError() error {
    var e *MyError = nil
    if e == nil {
        return nil  // Return actual nil
    }
    return e
}
```

### Pitfall 2: Interface pollution

```go
// Bad: Interface for every type
type UserServiceInterface interface {
    GetUser(id string) (*User, error)
    SaveUser(u *User) error
    DeleteUser(id string) error
    ListUsers() ([]*User, error)
    // ... 20 more methods
}

// Good: Small, focused interfaces
type UserGetter interface {
    GetUser(id string) (*User, error)
}

type UserSaver interface {
    SaveUser(u *User) error
}

// Define only what consumers need
```

### Pitfall 3: Pointer vs value receiver confusion

```go
type Counter struct{ n int }

// Value receiver
func (c Counter) Count() int { return c.n }

// Pointer receiver
func (c *Counter) Increment() { c.n++ }

type Incrementer interface {
    Increment()
}

var i Incrementer

i = &Counter{}  // OK: *Counter has Increment()
i = Counter{}   // ERROR: Counter does not have Increment()

// Rule: If ANY method has pointer receiver,
// only pointer satisfies interface
```

### Pitfall 4: Empty interface loses type safety

```go
// Bad: Losing type information
func Process(data interface{}) {
    // Must type assert everything
    switch v := data.(type) {
    case string:
        // ...
    }
}

// Better: Use generics (Go 1.18+) or specific interface
func Process[T Processable](data T) {
    data.Process()
}
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "encoding/json"
    "fmt"
    "sync"
    "time"
)

// Small, focused interfaces (consumer-defined)
type RouteReader interface {
    GetRoute(vrfID uint32, prefix string) (Route, bool)
    ListRoutes() []Route
}

type RouteWriter interface {
    AddRoute(Route) error
    DeleteRoute(vrfID uint32, prefix string) error
}

type RouteStore interface {
    RouteReader
    RouteWriter
}

// Domain type
type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

// Concrete implementation
type InMemoryRouteStore struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewInMemoryRouteStore() *InMemoryRouteStore {
    return &InMemoryRouteStore{
        routes: make(map[string]Route),
    }
}

func (s *InMemoryRouteStore) key(vrfID uint32, prefix string) string {
    return fmt.Sprintf("%d:%s", vrfID, prefix)
}

func (s *InMemoryRouteStore) GetRoute(vrfID uint32, prefix string) (Route, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    r, ok := s.routes[s.key(vrfID, prefix)]
    return r, ok
}

func (s *InMemoryRouteStore) ListRoutes() []Route {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    result := make([]Route, 0, len(s.routes))
    for _, r := range s.routes {
        result = append(result, r)
    }
    return result
}

func (s *InMemoryRouteStore) AddRoute(r Route) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.routes[s.key(r.VrfID, r.Prefix)] = r
    return nil
}

func (s *InMemoryRouteStore) DeleteRoute(vrfID uint32, prefix string) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    delete(s.routes, s.key(vrfID, prefix))
    return nil
}

// Handler accepts interface - easy to test and swap implementations
type RouteHandler struct {
    store RouteReader  // Only needs read operations
}

func NewRouteHandler(store RouteReader) *RouteHandler {
    return &RouteHandler{store: store}
}

func (h *RouteHandler) ServeRoute(vrfID uint32, prefix string) ([]byte, error) {
    route, ok := h.store.GetRoute(vrfID, prefix)
    if !ok {
        return nil, fmt.Errorf("route not found: %d:%s", vrfID, prefix)
    }
    return json.Marshal(route)
}

// Updater accepts interface with write capability
type RouteUpdater struct {
    store RouteStore  // Needs both read and write
}

func NewRouteUpdater(store RouteStore) *RouteUpdater {
    return &RouteUpdater{store: store}
}

func (u *RouteUpdater) UpdateNextHop(vrfID uint32, prefix, newNextHop string) error {
    route, ok := u.store.GetRoute(vrfID, prefix)
    if !ok {
        return fmt.Errorf("route not found")
    }
    route.NextHop = newNextHop
    return u.store.AddRoute(route)
}

// Mock for testing - implements RouteReader
type MockRouteReader struct {
    routes map[string]Route
}

func (m *MockRouteReader) GetRoute(vrfID uint32, prefix string) (Route, bool) {
    r, ok := m.routes[fmt.Sprintf("%d:%s", vrfID, prefix)]
    return r, ok
}

func (m *MockRouteReader) ListRoutes() []Route {
    result := make([]Route, 0, len(m.routes))
    for _, r := range m.routes {
        result = append(result, r)
    }
    return result
}

// Interface for observability
type RouteNotifier interface {
    OnRouteAdded(Route)
    OnRouteDeleted(vrfID uint32, prefix string)
}

// Observed store wraps another store and notifies
type ObservedRouteStore struct {
    RouteStore  // Embed interface
    notifier    RouteNotifier
}

func (o *ObservedRouteStore) AddRoute(r Route) error {
    if err := o.RouteStore.AddRoute(r); err != nil {
        return err
    }
    o.notifier.OnRouteAdded(r)
    return nil
}

func (o *ObservedRouteStore) DeleteRoute(vrfID uint32, prefix string) error {
    if err := o.RouteStore.DeleteRoute(vrfID, prefix); err != nil {
        return err
    }
    o.notifier.OnRouteDeleted(vrfID, prefix)
    return nil
}

// Logger implements RouteNotifier
type LogNotifier struct{}

func (LogNotifier) OnRouteAdded(r Route) {
    fmt.Printf("Route added: %+v\n", r)
}

func (LogNotifier) OnRouteDeleted(vrfID uint32, prefix string) {
    fmt.Printf("Route deleted: %d:%s\n", vrfID, prefix)
}

func main() {
    // Create base store
    baseStore := NewInMemoryRouteStore()
    
    // Wrap with observer
    store := &ObservedRouteStore{
        RouteStore: baseStore,
        notifier:   LogNotifier{},
    }
    
    // Add routes
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
    }
    
    for _, r := range routes {
        store.AddRoute(r)
    }
    
    // Handler only needs RouteReader
    handler := NewRouteHandler(store)
    data, _ := handler.ServeRoute(1, "10.0.0.0/24")
    fmt.Printf("Served: %s\n", data)
    
    // Updater needs full RouteStore
    updater := NewRouteUpdater(store)
    updater.UpdateNextHop(1, "10.0.0.0/24", "10.10.10.1")
    
    // Delete triggers notification
    store.DeleteRoute(1, "10.0.1.0/24")
    
    // Can use mock for testing
    mock := &MockRouteReader{
        routes: map[string]Route{
            "1:test/32": {VrfID: 1, Prefix: "test/32", NextHop: "mock"},
        },
    }
    testHandler := NewRouteHandler(mock)
    data, _ = testHandler.ServeRoute(1, "test/32")
    fmt.Printf("From mock: %s\n", data)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    INTERFACE DESIGN RULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. KEEP INTERFACES SMALL                                              │
│      • 1-3 methods is ideal                                             │
│      • "The bigger the interface, the weaker the abstraction"           │
│                                                                         │
│   2. DEFINE INTERFACES AT CONSUMER, NOT PRODUCER                        │
│      • Consumer knows what it needs                                     │
│      • Enables decoupling between packages                              │
│                                                                         │
│   3. ACCEPT INTERFACES, RETURN STRUCTS                                  │
│      • Functions accept interfaces for flexibility                      │
│      • Return concrete types for clarity                                │
│                                                                         │
│   4. DON'T EXPORT INTERFACES FOR IMPLEMENTATION                         │
│      • Don't force implementers to import your interface                │
│      • Let consumers define their own interfaces                        │
│                                                                         │
│   5. BEWARE THE NIL INTERFACE TRAP                                      │
│      • (T, nil) != nil when T is known                                  │
│      • Return explicit nil when needed                                  │
│                                                                         │
│   6. USE INTERFACE EMBEDDING FOR COMPOSITION                            │
│      • Combine small interfaces into larger ones                        │
│      • io.ReadWriteCloser = Reader + Writer + Closer                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 接口核心概念

**Go 的接口是隐式实现的——类型不需要声明它实现了哪个接口。**

### 接口满足规则

```go
type Writer interface {
    Write([]byte) (int, error)
}

// 任何有 Write 方法的类型都自动实现 Writer
// 不需要 "implements" 关键字
```

### 关键设计原则

| 原则 | 说明 |
|------|------|
| 小接口 | 1-3 个方法最理想 |
| 消费者定义接口 | 在使用接口的地方定义，而非实现的地方 |
| 接受接口，返回结构体 | 函数参数用接口，返回值用具体类型 |
| 不要导出实现用的接口 | 让消费者定义自己需要的接口 |

### nil 接口陷阱

```go
var e *MyError = nil
var err error = e   // err != nil !

// err 是 (*MyError, nil)，不是 (nil, nil)
// 接口值包含类型信息，所以不是 nil
```

### 接口组合

```go
type Reader interface { Read([]byte) (int, error) }
type Writer interface { Write([]byte) (int, error) }
type ReadWriter interface {
    Reader  // 嵌入
    Writer  // 嵌入
}
```

### 最佳实践

1. **保持接口小巧**
2. **在消费者端定义接口**
3. **接受接口，返回结构体**
4. **使用接口嵌入组合小接口**
5. **注意 nil 接口陷阱**

