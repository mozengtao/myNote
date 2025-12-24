# Topic 12: Escape Analysis (Stack vs Heap)

## 1. Problem It Solves (Engineering Motivation)

Memory allocation performance:
- **Stack allocation**: ~1 CPU cycle (just move stack pointer)
- **Heap allocation**: ~100+ CPU cycles (complex bookkeeping)
- **GC overhead**: Only applies to heap allocations

The question: **Where should a variable be allocated?**

In C: programmer decides (local variables = stack, malloc = heap)
In Go: **compiler decides automatically via escape analysis**

```
┌─────────────────────────────────────────────────────────────────┐
│                    Stack vs Heap Allocation                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Stack (fast, automatic):        Heap (slower, GC-managed):      │
│                                                                  │
│  ┌─────────────────────┐        ┌─────────────────────────────┐ │
│  │ main() frame        │        │                             │ │
│  │ ┌─────────────────┐ │        │  ┌─────┐  ┌─────┐  ┌─────┐ │ │
│  │ │ x = 42          │ │        │  │ obj1│  │ obj2│  │ obj3│ │ │
│  │ │ y = 3.14        │ │        │  └─────┘  └─────┘  └─────┘ │ │
│  │ └─────────────────┘ │        │                             │ │
│  ├─────────────────────┤        │  Allocated objects         │ │
│  │ foo() frame         │        │  GC tracks and reclaims    │ │
│  │ ┌─────────────────┐ │        │                             │ │
│  │ │ temp = "hello"  │ │        └─────────────────────────────┘ │
│  │ └─────────────────┘ │                                        │
│  └─────────────────────┘                                        │
│                                                                  │
│  Stack: LIFO, freed when        Heap: Objects live until        │
│  function returns               no more references              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 编译器使用"逃逸分析"来决定变量是分配在栈上还是堆上。如果变量在函数返回后仍然需要存在（"逃逸"出函数作用域），它就会被分配到堆上。否则，它分配在栈上，速度更快，也不需要 GC 回收。

## 2. Core Idea and Mental Model

**Escape analysis rule**: If a pointer to a variable could outlive the function, the variable "escapes" to the heap.

Escape scenarios:
1. Returning a pointer to a local variable
2. Storing a pointer in a global variable
3. Sending a pointer to a channel
4. Storing a pointer in a slice/map that escapes
5. Capturing in a closure that escapes

```
┌─────────────────────────────────────────────────────────────────┐
│                    Escape Analysis Decision                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  func example() *int {           Compiler thinks:               │
│      x := 42                     "x is returned as pointer"     │
│      return &x   ←───────────    "x escapes to heap"            │
│  }                                                               │
│                                                                  │
│  func example2() int {           Compiler thinks:               │
│      x := 42                     "x is returned by value"       │
│      return x   ←───────────     "x can stay on stack"          │
│  }                                                               │
│                                                                  │
│  func example3() {               Compiler thinks:               │
│      x := 42                     "x never leaves function"      │
│      fmt.Println(x)              "x can stay on stack"          │
│  }                                                               │
│                                                                  │
│  func example4() {               Compiler thinks:               │
│      x := 42                     "&x passed to Println"         │
│      fmt.Println(&x) ←───────    "interface{} might escape"     │
│  }                               "x escapes to heap"            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Checking Escape Analysis

```bash
# Show escape analysis decisions
go build -gcflags="-m" main.go

# More verbose
go build -gcflags="-m -m" main.go

# Output examples:
# ./main.go:10:6: x escapes to heap
# ./main.go:15:6: y does not escape
```

### Common Escape Patterns

```go
// ESCAPES: pointer returned
func newInt() *int {
    x := 42
    return &x  // x escapes to heap
}

// DOES NOT ESCAPE: value returned
func getInt() int {
    x := 42
    return x  // x stays on stack
}

// ESCAPES: stored in global
var global *int
func storeGlobal() {
    x := 42
    global = &x  // x escapes to heap
}

// ESCAPES: interface conversion (sometimes)
func printIt() {
    x := 42
    fmt.Println(x)  // x may escape (interface{})
}

// DOES NOT ESCAPE: passed to non-escaping parameter
func sum(a, b int) int {
    return a + b  // a, b stay on stack
}
```

### Controlling Escapes

```go
// Preallocate to avoid escapes in loops
func processItems(items []Item) {
    var result Result  // Stack allocated (doesn't escape)
    for _, item := range items {
        process(item, &result)  // Pass pointer, avoid copy
    }
}

// Use sync.Pool for frequently allocated objects
var bufPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // vrfId is a local variable - likely stays on stack
    vrfId := route.VrfId
    if vrfId == InvalidVrfId {
        vrfId = DefaultVrfId
    }
    
    // This struct literal with & escapes to heap
    // (returned from function)
    return &routermgrpb.RouteActionResponse{Success: success}, nil
}

// Struct used as map key - stored in heap (map is on heap)
VmcRoutes[route.VmcName][VmcRoute{
    VrfId:     vrfId,
    Address:   route.IpAddress,
    PrefixLen: route.PrefixLength,
    NextHop:   route.NextHopAddress,
    IsV6:      false,
}] = false
```

### Performance-Critical Code

```go
// Hot path: avoid allocations
func processPacket(packet []byte, stats *Stats) {
    // Don't allocate in the hot path
    header := packet[:20]  // Slice header on stack, data shared
    
    // Don't use interface{} in hot paths
    // Bad: fmt.Printf("%d", header[0])  // allocates
    // Good: direct function call
    stats.PacketCount++
}

// Use build tags for optimization
//go:noinline  // Prevent inlining for benchmarking
func criticalPath() { ... }
```

## 5. Common Mistakes and Pitfalls

1. **Premature optimization**:
   ```go
   // Don't obsess over escape analysis
   // Most code is not performance-critical
   
   // Clear code (might escape):
   func NewUser(name string) *User {
       return &User{Name: name}
   }
   
   // vs Premature optimization (harder to use):
   func NewUser(name string, u *User) {
       u.Name = name
   }
   ```

2. **Interface{} causes escapes**:
   ```go
   // Escapes because of interface{}
   fmt.Println(x)    // x boxed in interface{}
   fmt.Printf("%v", x)
   
   // If performance-critical, avoid reflection/interfaces
   io.WriteString(w, strconv.Itoa(x))  // Less allocation
   ```

3. **Large arrays on stack**:
   ```go
   // Bad: 1MB on stack (may cause stack overflow)
   func process() {
       var buffer [1024*1024]byte
       // ...
   }
   
   // Better: let compiler decide (escape to heap)
   func process() {
       buffer := make([]byte, 1024*1024)
       // ...
   }
   ```

4. **Closures capturing variables**:
   ```go
   func createHandlers() []func() {
       var handlers []func()
       for i := 0; i < 10; i++ {
           i := i  // Shadow - each closure gets its own copy
           handlers = append(handlers, func() {
               fmt.Println(i)  // i escapes (captured by closure)
           })
       }
       return handlers
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C/C++ | Go |
|--------|-------|-----|
| Stack allocation | Local variables | Compiler decides |
| Heap allocation | malloc/new | Compiler decides |
| Decision maker | Programmer | Escape analysis |
| Visibility | Explicit | Hidden (use -gcflags="-m") |
| Optimization | Manual | Automatic |

### C: Explicit Control

```c
// C: Programmer controls allocation
void stack_only() {
    int x = 42;        // Stack
    int arr[100];      // Stack
}

void heap_allocated() {
    int* x = malloc(sizeof(int));  // Heap
    *x = 42;
    // Must free(x)!
}

// Returning pointer to local - BUG in C!
int* dangerous() {
    int x = 42;
    return &x;  // UNDEFINED BEHAVIOR
}
```

### Go: Compiler Decides

```go
// Go: Same syntax, compiler decides
func autoStack() {
    x := 42         // Probably stack
    arr := [100]int{} // Probably stack
}

func autoHeap() *int {
    x := 42
    return &x  // x moved to heap automatically (SAFE)
}
```

## 7. A Small But Complete Go Example

```go
// escape.go - Demonstrating escape analysis
package main

import "fmt"

// Check with: go build -gcflags="-m" escape.go

// ========== DOES NOT ESCAPE ==========

//go:noinline
func noEscape1() int {
    x := 42
    return x  // Value returned, x stays on stack
}

//go:noinline
func noEscape2(a, b int) int {
    sum := a + b  // Local variable, stays on stack
    return sum
}

//go:noinline
func noEscape3() {
    x := [100]int{}  // Array on stack
    for i := range x {
        x[i] = i
    }
    _ = x[50]
}

// ========== ESCAPES TO HEAP ==========

//go:noinline
func escapes1() *int {
    x := 42
    return &x  // x escapes: pointer returned
}

//go:noinline
func escapes2() []int {
    x := make([]int, 100)
    return x  // slice header returned, backing array escapes
}

var globalPtr *int

//go:noinline
func escapes3() {
    x := 42
    globalPtr = &x  // x escapes: stored in global
}

//go:noinline
func escapes4() {
    x := 42
    fmt.Println(x)  // x escapes: interface{} conversion
}

type BigStruct struct {
    data [1000]int
}

//go:noinline
func escapes5() *BigStruct {
    s := BigStruct{}
    return &s  // Large struct escapes
}

// ========== DEMONSTRATING BEHAVIOR ==========

func main() {
    fmt.Println("=== No Escape Examples ===")
    fmt.Println("noEscape1():", noEscape1())
    fmt.Println("noEscape2(3, 4):", noEscape2(3, 4))
    noEscape3()
    fmt.Println("noEscape3(): completed")

    fmt.Println("\n=== Escape Examples ===")
    ptr := escapes1()
    fmt.Println("escapes1():", *ptr)
    
    slice := escapes2()
    fmt.Println("escapes2():", len(slice))
    
    escapes3()
    fmt.Println("escapes3(): globalPtr =", *globalPtr)
    
    escapes4()  // Prints via fmt.Println
    
    big := escapes5()
    fmt.Println("escapes5(): created BigStruct at", &big.data[0])

    fmt.Println("\n=== Run with go build -gcflags=\"-m\" to see analysis ===")
}
```

Build with escape analysis output:
```bash
$ go build -gcflags="-m" escape.go 2>&1 | head -20

./escape.go:12:6: can inline noEscape1
./escape.go:13:2: x does not escape
./escape.go:18:6: can inline noEscape2
./escape.go:19:2: sum does not escape
./escape.go:37:6: x escapes to heap
./escape.go:44:6: make([]int, 100) escapes to heap
./escape.go:52:2: x escapes to heap
./escape.go:58:13: x escapes to heap
./escape.go:65:2: &s escapes to heap
```

---

**Summary**: Escape analysis is the compiler's automatic decision about stack vs heap allocation. Variables that might be accessed after a function returns "escape" to the heap. This is transparent to the programmer but understanding it helps write more efficient code. Use `go build -gcflags="-m"` to see decisions. For most code, trust the compiler; optimize only when profiling shows allocation is a bottleneck.

