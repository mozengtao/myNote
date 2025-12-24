# Topic 11: Garbage Collection: How It Works Conceptually

## 1. Problem It Solves (Engineering Motivation)

Manual memory management problems:
- **Use-after-free**: Accessing memory after deallocation
- **Double-free**: Freeing the same memory twice
- **Memory leaks**: Forgetting to free memory
- **Dangling pointers**: References to freed memory
- **Developer time**: 20-40% of C/C++ development is memory management

```
┌─────────────────────────────────────────────────────────────────┐
│                 Manual vs Automatic Memory                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C/C++ (Manual):                  Go (Automatic GC):             │
│                                                                  │
│  void* p = malloc(100);           p := make([]byte, 100)        │
│  // ... use p ...                 // ... use p ...              │
│  free(p);                         // (nothing - GC handles it)  │
│  // Don't use p anymore!          // Can still use p            │
│  // p is now dangling             // p valid until unreachable  │
│                                                                  │
│  Common bugs:                     No equivalent bugs:            │
│  - Forgot free() → leak           - GC reclaims unused          │
│  - free() twice → corruption      - GC tracks references        │
│  - Use after free → crash/vuln    - Cannot access freed mem     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
垃圾回收（GC）自动管理内存分配和释放。程序员只需要分配内存，不需要手动释放。GC 会追踪哪些内存仍在使用，自动回收不再使用的内存。这消除了内存泄漏、悬垂指针等一整类 bug。

## 2. Core Idea and Mental Model

Go's GC is a **concurrent, tri-color, mark-and-sweep collector**.

**Conceptual model**:
1. **Allocate**: Your code allocates memory (via `new`, `make`, literals)
2. **Use**: Memory is reachable through variables, pointers, etc.
3. **Unreachable**: When no path exists from "roots" to the memory
4. **Collect**: GC finds unreachable memory and reclaims it

```
┌─────────────────────────────────────────────────────────────────┐
│                    Garbage Collection Phases                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. MARK Phase:                                                  │
│     Start from "roots" (stack variables, globals)                │
│     Trace all reachable objects                                  │
│                                                                  │
│     Roots                   Heap                                 │
│     ┌─────┐               ┌─────┐   ┌─────┐   ┌─────┐           │
│     │ var1├──────────────►│  A  ├──►│  B  │   │  C  │           │
│     │ var2├──────────┐    └─────┘   └──┬──┘   └─────┘           │
│     └─────┘          │                 │         ▲               │
│                      │                 ▼         │ (no path)     │
│                      │              ┌─────┐      │               │
│                      └─────────────►│  D  │──────┘               │
│                                     └─────┘       unreachable!   │
│                                                                  │
│  2. SWEEP Phase:                                                 │
│     Reclaim unmarked objects (C in this example)                 │
│                                                                  │
│  Tri-color marking:                                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ WHITE: Not yet seen (candidates for collection)           │  │
│  │ GRAY:  Seen but not fully scanned                         │  │
│  │ BLACK: Scanned, definitely reachable                      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Allocation (GC manages all of these)

```go
// Various ways to allocate - all GC-managed
p := new(Point)              // Allocates zero Point, returns *Point
s := make([]int, 10)         // Allocates slice + backing array
m := make(map[string]int)    // Allocates map structure
c := make(chan int, 5)       // Allocates channel buffer

// Composite literals
pt := &Point{X: 1, Y: 2}     // Allocates Point, returns pointer
data := []byte{1, 2, 3, 4}   // Allocates slice + array
```

### No Deallocation

```go
// There is NO:
// - free(p)
// - delete p
// - p.Dispose()
// - p.Release()

// Just stop using it:
func example() {
    data := make([]byte, 1000000)  // 1MB allocated
    // use data...
    
    data = nil  // Optional: helps GC know immediately
    // Or just let 'data' go out of scope
}
// After function returns, 'data' is unreachable
// GC will reclaim it eventually
```

### GC Tuning (rarely needed)

```go
import "runtime"

// Get GC statistics
var stats runtime.MemStats
runtime.ReadMemStats(&stats)
fmt.Printf("Heap size: %d bytes\n", stats.HeapAlloc)

// Force GC (usually not needed)
runtime.GC()

// Tune GC aggressiveness
// GOGC=100 means GC runs when heap doubles (default)
// GOGC=200 means GC runs when heap triples
// GOGC=off disables GC (dangerous)
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
func StartGrpcServer() {
    // These allocations are automatically managed
    RouterAddresses = make(map[uint32]map[int]RouterAddress)
    VmcAddresses = make(map[uint32]map[string]map[int]VmcAddress)
    VmcRoutes = make(map[string]map[VmcRoute]bool)
    
    grpcServer := grpc.NewServer()  // Allocates complex structure
    routermgrpb.RegisterRouterMgrServer(grpcServer, &routermgrSrv)
    grpcServer.Serve(lis)
    
    // When server stops, all these become unreachable
    // GC will clean up automatically
}

func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // 'route' is allocated by gRPC, managed by GC
    // Response is allocated here, passed to gRPC, managed by GC
    return &routermgrpb.RouteActionResponse{Success: success}, nil
    // No need to free 'route' or the response
}
```

### Common Patterns

```go
// Pattern 1: Request handling (allocate freely)
func handleRequest(w http.ResponseWriter, r *http.Request) {
    // Allocate as needed - GC handles cleanup
    data := make([]byte, 1024)
    json.Unmarshal(data, &request)
    
    response := processRequest(request)
    json.Marshal(response)
    // All allocations cleaned up after request
}

// Pattern 2: Object pools (reduce GC pressure)
var bufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}

func useBuffer() {
    buf := bufferPool.Get().([]byte)
    defer bufferPool.Put(buf)
    // Use buf...
}
```

## 5. Common Mistakes and Pitfalls

1. **Keeping references longer than needed**:
   ```go
   var cache = make(map[string]*BigData)  // Global cache
   
   func process(key string, data *BigData) {
       cache[key] = data  // Now 'data' lives forever!
       // Even after process() returns, data can't be GC'd
   }
   
   // Fix: Remove from cache when done
   func cleanup(key string) {
       delete(cache, key)
   }
   ```

2. **Slice gotcha - holding onto backing array**:
   ```go
   func readHeader(data []byte) []byte {
       return data[:10]  // Keeps entire 'data' alive!
   }
   
   // Fix: copy if you only need a small part
   func readHeader(data []byte) []byte {
       header := make([]byte, 10)
       copy(header, data[:10])
       return header
   }
   ```

3. **Finalizers (avoid them)**:
   ```go
   // Rarely needed, hard to use correctly
   runtime.SetFinalizer(obj, func(o *MyObj) {
       o.Cleanup()  // Called when GC is about to collect
   })
   // Problems: timing unpredictable, may never run
   ```

4. **Excessive allocation in hot paths**:
   ```go
   // Bad: allocates on every call
   func format(n int) string {
       return fmt.Sprintf("%d", n)  // Allocates
   }
   
   // Better for hot paths: use strconv
   func format(n int) string {
       return strconv.Itoa(n)  // Less allocation
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C/C++ | Go |
|--------|-------|-----|
| Allocation | malloc/new | new/make/literals |
| Deallocation | free/delete | Automatic |
| Dangling pointers | Possible | Impossible |
| Memory leaks | Common | Rare (but possible) |
| Latency | Predictable | GC pauses (short in Go) |
| Control | Full | Limited |

### C Memory Management

```c
// C: Manual, error-prone
char* data = malloc(1024);
if (data == NULL) {
    // Handle error
}

// ... use data ...

free(data);
data = NULL;  // Defensive: prevent use-after-free

// Bugs that Go prevents:
// free(data); free(data);  // Double free
// use(data);               // Use after free
```

### Go Equivalent

```go
// Go: Automatic, safe
data := make([]byte, 1024)
// No NULL check needed (panics on OOM)

// ... use data ...

// No free() needed
// Cannot double-free
// Cannot use-after-free (memory valid until unreachable)
```

### GC Pause Times

Go 1.5+: Typically < 1ms pauses
Go 1.8+: Typically < 100μs pauses

For most applications, GC pauses are negligible.

## 7. A Small But Complete Go Example

```go
// gc_demo.go - Demonstrating garbage collection behavior
package main

import (
    "fmt"
    "runtime"
    "time"
)

type BigObject struct {
    data [1024 * 1024]byte  // 1MB
    id   int
}

func printMemStats(label string) {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    fmt.Printf("%s - Heap: %.2f MB, GC cycles: %d\n",
        label, float64(m.HeapAlloc)/1024/1024, m.NumGC)
}

func allocateAndForget() {
    // Allocate objects that become unreachable
    for i := 0; i < 10; i++ {
        obj := &BigObject{id: i}
        _ = obj  // Use it briefly
    }
    // All 10 objects are now unreachable
}

func allocateAndKeep(keeper *[]*BigObject) {
    // Allocate objects and keep references
    for i := 0; i < 10; i++ {
        obj := &BigObject{id: i}
        *keeper = append(*keeper, obj)
    }
    // Objects are still reachable through 'keeper'
}

func main() {
    printMemStats("Start")
    
    // Allocate objects that become garbage
    fmt.Println("\n=== Allocating forgettable objects ===")
    allocateAndForget()
    printMemStats("After allocateAndForget")
    
    // Force GC to see the effect
    runtime.GC()
    printMemStats("After GC")
    
    // Allocate objects we keep
    fmt.Println("\n=== Allocating kept objects ===")
    var kept []*BigObject
    allocateAndKeep(&kept)
    printMemStats("After allocateAndKeep")
    
    // GC won't reclaim kept objects
    runtime.GC()
    printMemStats("After GC (objects still referenced)")
    
    // Release references
    fmt.Println("\n=== Releasing references ===")
    kept = nil  // Now the 10 objects are unreachable
    runtime.GC()
    printMemStats("After releasing and GC")
    
    // Demonstrate GC runs automatically
    fmt.Println("\n=== Automatic GC ===")
    for i := 0; i < 50; i++ {
        _ = &BigObject{id: i}
    }
    // GC runs automatically when heap grows
    printMemStats("After many allocations")
    
    // GC timing
    fmt.Println("\n=== GC Timing ===")
    start := time.Now()
    runtime.GC()
    fmt.Printf("Explicit GC took: %v\n", time.Since(start))
}
```

Output (approximate):
```
Start - Heap: 0.12 MB, GC cycles: 0

=== Allocating forgettable objects ===
After allocateAndForget - Heap: 10.12 MB, GC cycles: 0
After GC - Heap: 0.12 MB, GC cycles: 1

=== Allocating kept objects ===
After allocateAndKeep - Heap: 10.12 MB, GC cycles: 1
After GC (objects still referenced) - Heap: 10.12 MB, GC cycles: 2

=== Releasing references ===
After releasing and GC - Heap: 0.12 MB, GC cycles: 3

=== Automatic GC ===
After many allocations - Heap: 0.15 MB, GC cycles: 7

=== GC Timing ===
Explicit GC took: 45.123µs
```

---

**Summary**: Go's garbage collector automatically reclaims memory that's no longer reachable. You allocate freely and don't worry about deallocation. The GC uses concurrent marking to minimize pauses (typically <1ms). This eliminates use-after-free, double-free, and most memory leak bugs. The trade-off is some CPU overhead for GC work and occasional pauses, but for most applications, this is an excellent trade-off for safety and productivity.

