# Topic 13: Object Lifetime and Ownership in Go

## 1. Problem It Solves (Engineering Motivation)

Ownership questions that plague C/C++ code:
- Who is responsible for freeing this memory?
- How long should this object live?
- Can I hold onto this pointer?
- Is this pointer still valid?

```
┌─────────────────────────────────────────────────────────────────┐
│                 Ownership Mental Models                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C++/Rust: Explicit Ownership                                    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  unique_ptr<T>  → One owner, auto-delete                │    │
│  │  shared_ptr<T>  → Reference counted                     │    │
│  │  T&             → Non-owning reference                  │    │
│  │  T*             → Raw pointer (manual management)       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Go: No Explicit Ownership                                       │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  *T  → Pointer (share freely, GC handles cleanup)       │    │
│  │  T   → Value (copy, independent lifetime)               │    │
│  │                                                          │    │
│  │  No unique_ptr, no shared_ptr, no weak_ptr              │    │
│  │  All objects live as long as they're reachable          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
在 C++ 中，你必须明确谁"拥有"对象（负责释放它）。Go 没有所有权概念——对象只要可达就一直存在。GC 负责清理。这简化了代码，但你仍然需要理解对象生命周期以避免资源泄漏。

## 2. Core Idea and Mental Model

**Go's lifetime rules**:
1. Objects live as long as they're reachable from a "root"
2. Roots are: goroutine stacks, global variables, CPU registers
3. When unreachable → eligible for GC
4. Resources (files, connections) need explicit cleanup

```
┌─────────────────────────────────────────────────────────────────┐
│                    Object Lifetime in Go                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Reachability determines lifetime:                               │
│                                                                  │
│  Roots              Reachable Objects     Unreachable           │
│  ┌─────┐           ┌─────┐                ┌─────┐               │
│  │stack├──────────►│  A  ├──────────┐     │  X  │ ← GC will     │
│  │var  │           └─────┘          │     └─────┘   reclaim     │
│  └─────┘                            │                            │
│  ┌─────┐           ┌─────┐          │     ┌─────┐               │
│  │global├─────────►│  B  ├──────────┤     │  Y  │ ← GC will     │
│  │var   │          └─────┘          │     └─────┘   reclaim     │
│  └─────┘                            ▼                            │
│                                 ┌─────┐                          │
│                                 │  C  │                          │
│                                 └─────┘                          │
│                                                                  │
│  Time ──────────────────────────────────────────────────────►   │
│                                                                  │
│  func example() {                                                │
│      obj := new(T)    // obj created                            │
│      // ... use obj                                              │
│  }                    // obj unreachable after return           │
│  // GC reclaims obj eventually                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Variable Scope = Approximate Lifetime

```go
func processData() {
    // 'data' lives at least until end of function
    data := make([]byte, 1000)
    
    // 'temp' lives at least until end of block
    if condition {
        temp := transform(data)
        save(temp)
    }
    // temp unreachable here
    
    // data still reachable
    finalize(data)
}
// data unreachable here
```

### Extended Lifetime via Closure

```go
func createCounter() func() int {
    count := 0  // count "escapes" via closure
    return func() int {
        count++  // closure captures count
        return count
    }
}

// count lives as long as the returned function lives
counter := createCounter()
counter()  // count still alive
counter()  // count still alive
```

### Resources Need Explicit Cleanup

```go
// Memory: automatic cleanup
data := make([]byte, 1000000)
// GC handles cleanup

// Resources: MUST cleanup manually
file, _ := os.Open("data.txt")
defer file.Close()  // REQUIRED

conn, _ := net.Dial("tcp", "server:80")
defer conn.Close()  // REQUIRED

db, _ := sql.Open("postgres", connStr)
defer db.Close()  // REQUIRED
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
func StartGrpcServer() {
    // These maps live for the entire server lifetime
    // They're global variables (roots)
    RouterAddresses = make(map[uint32]map[int]RouterAddress)
    VmcAddresses = make(map[uint32]map[string]map[int]VmcAddress)
    VmcRoutes = make(map[string]map[VmcRoute]bool)
    
    // Listener needs explicit close
    lis, err := net.Listen("tcp", GrpcPort)
    if err != nil {
        return
    }
    defer lis.Close()  // Cleanup resource
    
    // Server lifetime tied to Serve() call
    grpcServer := grpc.NewServer()
    grpcServer.Serve(lis)  // Blocks until server stops
}

// Request objects have short lifetime
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // 'route' lives for this request only
    // 'response' returned to caller, lives longer
    return &routermgrpb.RouteActionResponse{Success: success}, nil
}
```

### Object Pools for Performance

```go
// Reuse objects instead of letting GC reclaim them
var bufferPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}

func handleRequest() {
    buf := bufferPool.Get().([]byte)
    defer bufferPool.Put(buf)  // Return to pool instead of GC
    
    // Use buf...
}
```

## 5. Common Mistakes and Pitfalls

1. **Accidental lifetime extension**:
   ```go
   var cache = make(map[string]*BigData)
   
   func processAndCache(key string) {
       data := loadBigData()
       cache[key] = data  // data now lives forever!
   }
   
   // Fix: implement cache eviction
   func (c *Cache) Set(key string, data *BigData, ttl time.Duration) {
       c.data[key] = data
       time.AfterFunc(ttl, func() { c.Delete(key) })
   }
   ```

2. **Resource leaks (not memory)**:
   ```go
   // Bug: goroutine leak
   func startWorker() {
       go func() {
           for {
               data := <-ch  // Blocks forever if ch never closed
               process(data)
           }
       }()
   }
   
   // Fix: use context for cancellation
   func startWorker(ctx context.Context) {
       go func() {
           for {
               select {
               case data := <-ch:
                   process(data)
               case <-ctx.Done():
                   return  // Goroutine exits
               }
           }
       }()
   }
   ```

3. **Holding onto slices longer than needed**:
   ```go
   // Entire backing array kept alive
   var headers [][]byte
   
   func processFile(data []byte) {
       header := data[:100]
       headers = append(headers, header)  // Keeps entire data alive!
   }
   
   // Fix: copy the needed portion
   func processFile(data []byte) {
       header := make([]byte, 100)
       copy(header, data[:100])
       headers = append(headers, header)
   }
   ```

4. **Finalizers are not destructors**:
   ```go
   // Don't rely on finalizers for cleanup
   type Resource struct {
       file *os.File
   }
   
   func NewResource(path string) *Resource {
       r := &Resource{file: mustOpen(path)}
       runtime.SetFinalizer(r, func(r *Resource) {
           r.file.Close()  // May never run!
       })
       return r
   }
   
   // Correct: explicit Close method
   func (r *Resource) Close() error {
       return r.file.Close()
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C++ | Rust | Go |
|--------|-----|------|-----|
| Memory ownership | Manual/smart pointers | Enforced by compiler | No ownership (GC) |
| Lifetime tracking | Manual | Compiler-checked | Automatic (reachability) |
| Resource cleanup | RAII/destructors | Drop trait | defer + explicit Close |
| Dangling references | Possible | Prevented | Impossible (GC) |

### C++ RAII

```cpp
class FileReader {
    FILE* file;
public:
    FileReader(const char* path) : file(fopen(path, "r")) {}
    ~FileReader() { fclose(file); }  // Destructor = cleanup
};

void process() {
    FileReader reader("data.txt");
    // ... use reader ...
}  // ~FileReader() called automatically
```

### Go Equivalent

```go
func process() {
    file, _ := os.Open("data.txt")
    defer file.Close()  // Explicit defer
    // ... use file ...
}  // file.Close() called at function exit
```

Go's approach is more explicit about cleanup but simpler overall (no destructors, no move semantics, no ownership transfer).

## 7. A Small But Complete Go Example

```go
// lifetime.go - Demonstrating object lifetime patterns
package main

import (
    "context"
    "fmt"
    "runtime"
    "sync"
    "time"
)

// Resource that needs explicit cleanup
type Connection struct {
    id     int
    closed bool
}

func NewConnection(id int) *Connection {
    fmt.Printf("Connection %d: opened\n", id)
    return &Connection{id: id}
}

func (c *Connection) Close() {
    if !c.closed {
        c.closed = true
        fmt.Printf("Connection %d: closed\n", c.id)
    }
}

// Object pool for reusing objects
var connPool = sync.Pool{
    New: func() interface{} {
        return &Connection{}
    },
}

// Demonstrates scope-based lifetime
func scopeLifetime() {
    fmt.Println("\n=== Scope-Based Lifetime ===")
    
    outer := NewConnection(1)
    defer outer.Close()
    
    {
        inner := NewConnection(2)
        defer inner.Close()
        fmt.Println("Inside block")
    }  // inner.Close() called here
    
    fmt.Println("After block")
}  // outer.Close() called here

// Demonstrates extended lifetime via closure
func closureLifetime() func() int {
    fmt.Println("\n=== Closure-Extended Lifetime ===")
    
    count := 0  // count escapes, lives as long as returned func
    
    return func() int {
        count++
        fmt.Printf("Count: %d (still alive!)\n", count)
        return count
    }
}

// Demonstrates goroutine lifetime with context
func goroutineLifetime(ctx context.Context) {
    fmt.Println("\n=== Goroutine Lifetime ===")
    
    var wg sync.WaitGroup
    wg.Add(1)
    
    go func() {
        defer wg.Done()
        ticker := time.NewTicker(100 * time.Millisecond)
        defer ticker.Stop()
        
        for {
            select {
            case <-ticker.C:
                fmt.Println("Goroutine: tick")
            case <-ctx.Done():
                fmt.Println("Goroutine: cancelled, exiting")
                return
            }
        }
    }()
    
    time.Sleep(350 * time.Millisecond)
    fmt.Println("Main: waiting for goroutine...")
    wg.Wait()
}

// Demonstrates GC reclaiming unreachable objects
func gcLifetime() {
    fmt.Println("\n=== GC Lifetime ===")
    
    // Create object that becomes unreachable
    func() {
        data := make([]byte, 1024*1024)  // 1MB
        _ = data[0]
        // data becomes unreachable after this function
    }()
    
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    fmt.Printf("Before GC: %d KB heap\n", m.HeapAlloc/1024)
    
    runtime.GC()
    
    runtime.ReadMemStats(&m)
    fmt.Printf("After GC: %d KB heap\n", m.HeapAlloc/1024)
}

func main() {
    // Scope-based lifetime
    scopeLifetime()
    
    // Closure extends lifetime
    counter := closureLifetime()
    counter()
    counter()
    counter()
    
    // Goroutine with controlled lifetime
    ctx, cancel := context.WithCancel(context.Background())
    goroutineLifetime(ctx)
    cancel()  // Signal goroutine to exit
    
    time.Sleep(100 * time.Millisecond)  // Let goroutine finish
    
    // GC reclaims unreachable objects
    gcLifetime()
    
    fmt.Println("\n=== Summary ===")
    fmt.Println("1. Scope defines minimum lifetime")
    fmt.Println("2. Closures can extend lifetime")
    fmt.Println("3. Context controls goroutine lifetime")
    fmt.Println("4. GC reclaims unreachable objects")
    fmt.Println("5. Resources (files, conns) need explicit Close")
}
```

Output:
```
=== Scope-Based Lifetime ===
Connection 1: opened
Connection 2: opened
Inside block
Connection 2: closed
After block
Connection 1: closed

=== Closure-Extended Lifetime ===
Count: 1 (still alive!)
Count: 2 (still alive!)
Count: 3 (still alive!)

=== Goroutine Lifetime ===
Goroutine: tick
Goroutine: tick
Goroutine: tick
Main: waiting for goroutine...
Goroutine: cancelled, exiting

=== GC Lifetime ===
Before GC: 1150 KB heap
After GC: 120 KB heap

=== Summary ===
1. Scope defines minimum lifetime
2. Closures can extend lifetime
3. Context controls goroutine lifetime
4. GC reclaims unreachable objects
5. Resources (files, conns) need explicit Close
```

---

**Summary**: Go doesn't have ownership like Rust or C++. Objects live as long as they're reachable from roots (stacks, globals). GC handles memory, but resources (files, connections, goroutines) need explicit management. Use `defer` for cleanup, `context.Context` for goroutine lifetime, and object pools for performance. Don't rely on finalizers—always provide explicit `Close()` methods.

