# Topic 10: Methods and Receivers (Value vs Pointer)

## 1. Problem It Solves (Engineering Motivation)

In OOP languages, methods are defined inside classes:
```java
class Point {
    int x, y;
    void move(int dx, int dy) { x += dx; y += dy; }
}
```

Problems:
- Methods and data tightly coupled in class definition
- Can't add methods to types you don't own
- Inheritance hierarchies get complex
- `this`/`self` implicit, sometimes confusing

Go's approach: **Methods are functions with a receiver. Declared separately from type.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    Method Definition Models                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Java/C++:                        Go:                            │
│  ┌─────────────────────────┐     ┌─────────────────────────┐    │
│  │ class Point {           │     │ type Point struct {     │    │
│  │   int x, y;             │     │     X, Y int            │    │
│  │                         │     │ }                       │    │
│  │   void move(dx, dy) {   │     │                         │    │
│  │     x += dx;            │     │ func (p *Point) Move(   │    │
│  │     y += dy;            │     │     dx, dy int) {       │    │
│  │   }                     │     │     p.X += dx           │    │
│  │ }                       │     │     p.Y += dy           │    │
│  └─────────────────────────┘     │ }                       │    │
│                                  └─────────────────────────┘    │
│  Methods inside class            Methods outside type           │
│  Coupled together                Separated, can add later       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 的方法不在类型定义内部，而是通过"接收者"（receiver）参数将函数与类型关联。接收者可以是值类型（复制）或指针类型（共享）。这种设计更灵活，且明确显示方法是否修改接收者。

## 2. Core Idea and Mental Model

**A method is just a function with a receiver argument.**

```go
// This method:
func (p Point) Distance(q Point) float64 { ... }

// Is essentially:
func Distance(p Point, q Point) float64 { ... }
```

The receiver can be:
- **Value receiver** (`p Point`): Method receives a copy
- **Pointer receiver** (`p *Point`): Method receives a pointer

```
┌─────────────────────────────────────────────────────────────────┐
│                 Value vs Pointer Receiver                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Value Receiver:                  Pointer Receiver:              │
│  func (p Point) String() string   func (p *Point) Move(dx, dy)  │
│                                                                  │
│  ┌───────────┐                    ┌───────────┐                 │
│  │  caller   │                    │  caller   │                 │
│  │  pt       │                    │  pt       │                 │
│  │  {1, 2}   │                    │  {1, 2}   │                 │
│  └─────┬─────┘                    └─────┬─────┘                 │
│        │ copy                           │ address               │
│        ▼                                ▼                        │
│  ┌───────────┐                    ┌───────────┐                 │
│  │  method   │                    │  method   │                 │
│  │  p        │                    │  p        │                 │
│  │  {1, 2}   │ (independent)      │  → pt     │ (same data)     │
│  └───────────┘                    └───────────┘                 │
│                                                                  │
│  p.X = 10  // caller unchanged    p.X = 10  // caller modified  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Method Declaration

```go
type Point struct {
    X, Y float64
}

// Value receiver - works on copy
func (p Point) String() string {
    return fmt.Sprintf("(%v, %v)", p.X, p.Y)
}

// Pointer receiver - can modify receiver
func (p *Point) Scale(factor float64) {
    p.X *= factor
    p.Y *= factor
}

// Pointer receiver - shares state
func (p *Point) Translate(dx, dy float64) {
    p.X += dx
    p.Y += dy
}
```

### Method Invocation (Auto-dereference)

```go
// Go automatically takes address or dereferences
pt := Point{1, 2}
pt.Scale(2)      // Automatically: (&pt).Scale(2)

ptr := &Point{3, 4}
s := ptr.String()  // Automatically: (*ptr).String()
```

### Method Sets

```go
type T struct{}

func (t T) ValueMethod() {}
func (t *T) PointerMethod() {}

// Value has only value receiver methods
var v T
v.ValueMethod()    // OK
v.PointerMethod()  // OK: Go auto-takes address

// Pointer has both
var p *T
p.ValueMethod()    // OK: Go auto-dereferences
p.PointerMethod()  // OK

// But for INTERFACES, it matters:
type Iface interface {
    PointerMethod()
}

var i Iface = v   // ERROR: T doesn't implement (only *T does)
var i Iface = &v  // OK: *T implements Iface
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
// Pointer receiver for gRPC server methods
// - Allows sharing server state
// - Consistent with gRPC interface expectations
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    vrfId := route.VrfId
    if vrfId == InvalidVrfId {
        vrfId = DefaultVrfId
    }
    
    log.Debugf("Received add IPv4 route %s/%d %s Tag %d VrfId %d for VMC %s",
        route.IpAddress, route.PrefixLength, route.NextHopAddress, route.Tag, vrfId, route.VmcName)
    
    routeMutex.Lock()
    // ... modify shared state ...
    routeMutex.Unlock()
    
    success := MgmtdAddRouteIpv4(vrfId, route.PrefixLength, route.IpAddress, route.NextHopAddress, route.Tag)
    return &routermgrpb.RouteActionResponse{Success: success}, nil
}
```

### When to Use Which

```go
// Value receiver: small, immutable-like operations
type Money struct {
    Amount   int64
    Currency string
}

func (m Money) String() string {
    return fmt.Sprintf("%d %s", m.Amount, m.Currency)
}

func (m Money) Add(other Money) Money {  // Returns new value
    return Money{Amount: m.Amount + other.Amount, Currency: m.Currency}
}

// Pointer receiver: mutation or large types
type Account struct {
    Balance Money
    History []Transaction  // Large
}

func (a *Account) Deposit(m Money) {
    a.Balance = a.Balance.Add(m)
    a.History = append(a.History, Transaction{...})
}
```

### Consistency Rule

```go
// If ANY method needs pointer receiver, ALL should use pointer receiver
type Buffer struct {
    data []byte
}

func (b *Buffer) Write(p []byte) (int, error) {  // Needs pointer
    b.data = append(b.data, p...)
    return len(p), nil
}

func (b *Buffer) String() string {  // Use pointer for consistency
    return string(b.data)
}
// Even though String() could work with value receiver
```

## 5. Common Mistakes and Pitfalls

1. **Value receiver modifying state (doesn't work)**:
   ```go
   type Counter struct{ n int }
   
   func (c Counter) Increment() {  // BUG: value receiver
       c.n++  // Modifies copy, not original!
   }
   
   c := Counter{}
   c.Increment()
   fmt.Println(c.n)  // Still 0!
   
   // Fix: use pointer receiver
   func (c *Counter) Increment() {
       c.n++
   }
   ```

2. **Nil receiver handling**:
   ```go
   type List struct {
       head *Node
   }
   
   func (l *List) Len() int {
       if l == nil {
           return 0  // Handle nil gracefully
       }
       // ... count nodes ...
   }
   
   var l *List
   l.Len()  // Returns 0, doesn't panic
   ```

3. **Interface satisfaction with wrong receiver**:
   ```go
   type Sizer interface {
       Size() int
   }
   
   type File struct{ size int }
   
   func (f *File) Size() int {  // Pointer receiver
       return f.size
   }
   
   var f File
   var s Sizer = f   // ERROR: File doesn't implement Sizer
   var s Sizer = &f  // OK: *File implements Sizer
   ```

4. **Mixing receiver types in a type's method set**:
   ```go
   // Inconsistent (confusing)
   func (p Point) Method1() {}
   func (p *Point) Method2() {}
   func (p Point) Method3() {}
   
   // Better: pick one and stick with it
   func (p *Point) Method1() {}
   func (p *Point) Method2() {}
   func (p *Point) Method3() {}
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C++ | Go |
|--------|-----|-----|
| Method definition | Inside class | Outside type |
| `this`/`self` | Implicit | Explicit receiver |
| Receiver choice | Always pointer (`this*`) | Value or pointer |
| Const methods | `const` qualifier | Value receiver (conceptually) |
| Add methods later | No (except friend) | Yes (in same package) |

### C++ Methods

```cpp
class Point {
public:
    double x, y;
    
    // Method inside class
    void move(double dx, double dy) {
        this->x += dx;  // 'this' is implicit
        this->y += dy;
    }
    
    // Const method - doesn't modify
    double distanceToOrigin() const {
        return sqrt(x*x + y*y);
    }
};
```

### Go Equivalent

```go
type Point struct {
    X, Y float64
}

// Method outside type, explicit receiver
func (p *Point) Move(dx, dy float64) {
    p.X += dx  // receiver 'p' is explicit
    p.Y += dy
}

// Value receiver = conceptually "const"
func (p Point) DistanceToOrigin() float64 {
    return math.Sqrt(p.X*p.X + p.Y*p.Y)
}
```

### Linux Kernel Style (C)

```c
// C: No methods, use function with explicit struct pointer
struct point {
    double x, y;
};

void point_move(struct point *p, double dx, double dy) {
    p->x += dx;
    p->y += dy;
}

double point_distance(const struct point *p) {
    return sqrt(p->x * p->x + p->y * p->y);
}
```

Go is closer to C style: functions with explicit data pointer, just with nicer syntax.

## 7. A Small But Complete Go Example

```go
// methods.go - Demonstrating method receivers
package main

import (
    "fmt"
    "math"
)

// Point type
type Point struct {
    X, Y float64
}

// Value receiver: doesn't modify, works on copy
func (p Point) Distance(q Point) float64 {
    dx := p.X - q.X
    dy := p.Y - q.Y
    return math.Sqrt(dx*dx + dy*dy)
}

// Value receiver: returns new value (functional style)
func (p Point) Add(q Point) Point {
    return Point{p.X + q.X, p.Y + q.Y}
}

// Pointer receiver: modifies original
func (p *Point) Scale(factor float64) {
    p.X *= factor
    p.Y *= factor
}

// Pointer receiver: for consistency (even though could be value)
func (p *Point) String() string {
    return fmt.Sprintf("(%.2f, %.2f)", p.X, p.Y)
}

// Circle uses pointer receiver throughout
type Circle struct {
    Center Point
    Radius float64
}

func (c *Circle) Area() float64 {
    return math.Pi * c.Radius * c.Radius
}

func (c *Circle) Scale(factor float64) {
    c.Radius *= factor
}

func (c *Circle) Contains(p Point) bool {
    return c.Center.Distance(p) <= c.Radius
}

// NilSafe: methods can handle nil receivers
type Stack struct {
    items []int
}

func (s *Stack) Push(v int) {
    s.items = append(s.items, v)
}

func (s *Stack) Pop() (int, bool) {
    if s == nil || len(s.items) == 0 {
        return 0, false  // Graceful nil handling
    }
    v := s.items[len(s.items)-1]
    s.items = s.items[:len(s.items)-1]
    return v, true
}

func (s *Stack) Len() int {
    if s == nil {
        return 0
    }
    return len(s.items)
}

func main() {
    // Value receiver methods
    p1 := Point{0, 0}
    p2 := Point{3, 4}
    
    fmt.Printf("Distance from %s to %s: %.2f\n",
        (&p1).String(), (&p2).String(), p1.Distance(p2))
    
    p3 := p1.Add(p2)  // Returns new Point
    fmt.Printf("Sum: %s\n", (&p3).String())
    fmt.Printf("p1 unchanged: %s\n", (&p1).String())
    
    // Pointer receiver methods
    p2.Scale(2)  // Go auto-takes address
    fmt.Printf("p2 after Scale(2): %s\n", (&p2).String())
    
    // Circle with pointer receivers
    c := &Circle{Center: Point{0, 0}, Radius: 5}
    fmt.Printf("\nCircle: center=%s, radius=%.2f, area=%.2f\n",
        (&c.Center).String(), c.Radius, c.Area())
    
    testPoint := Point{3, 0}
    fmt.Printf("Contains %s: %v\n", (&testPoint).String(), c.Contains(testPoint))
    
    // Nil receiver handling
    fmt.Println("\n=== Nil Receiver Handling ===")
    var nilStack *Stack
    fmt.Printf("nilStack.Len(): %d\n", nilStack.Len())
    
    v, ok := nilStack.Pop()
    fmt.Printf("nilStack.Pop(): %d, %v\n", v, ok)
    
    // Regular stack usage
    stack := &Stack{}
    stack.Push(1)
    stack.Push(2)
    stack.Push(3)
    
    for stack.Len() > 0 {
        v, _ := stack.Pop()
        fmt.Printf("Popped: %d\n", v)
    }
    
    // Method values and expressions
    fmt.Println("\n=== Method Values ===")
    p := Point{1, 2}
    scaleMethod := (&p).Scale  // Bound method
    scaleMethod(10)
    fmt.Printf("After bound method call: %s\n", (&p).String())
}
```

Output:
```
Distance from (0.00, 0.00) to (3.00, 4.00): 5.00
Sum: (3.00, 4.00)
p1 unchanged: (0.00, 0.00)
p2 after Scale(2): (6.00, 8.00)

Circle: center=(0.00, 0.00), radius=5.00, area=78.54
Contains (3.00, 0.00): true

=== Nil Receiver Handling ===
nilStack.Len(): 0
nilStack.Pop(): 0, false
Popped: 3
Popped: 2
Popped: 1

=== Method Values ===
After bound method call: (10.00, 20.00)
```

---

**Summary**: Go methods are functions with a receiver parameter. Use value receivers for small types and methods that don't modify state. Use pointer receivers for methods that modify state or for large types. Be consistent within a type—if any method needs a pointer receiver, all should use pointer receiver. Unlike C++ where `this` is implicit, Go's explicit receiver makes the semantics clear: you always know whether you're working with a copy or sharing data.

