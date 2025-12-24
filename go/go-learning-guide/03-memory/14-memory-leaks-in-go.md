# Topic 14: When Memory Leaks Still Happen in Go

## 1. Problem It Solves (Engineering Motivation)

Common misconception: "GC means no memory leaks."

Reality: **Go has GC, but leaks are still possible.**

GC only reclaims unreachable memory. Problems occur when:
- Memory stays reachable but unused (logical leak)
- Resources other than memory are leaked (goroutines, file descriptors)
- Growing data structures aren't bounded

```
┌─────────────────────────────────────────────────────────────────┐
│                    Memory Leak Types in Go                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C Leak:                          Go "Leaks":                    │
│  ┌─────────────────────┐         ┌─────────────────────────┐    │
│  │ malloc() without    │         │ 1. Unbounded cache      │    │
│  │ free()              │         │ 2. Goroutine leak       │    │
│  │                     │         │ 3. Slice capacity held  │    │
│  │ GC would solve this │         │ 4. Global references    │    │
│  └─────────────────────┘         │ 5. Forgotten timers     │    │
│                                  │ 6. Deferred close fails │    │
│                                  └─────────────────────────┘    │
│                                                                  │
│  GC prevents:                     GC does NOT prevent:           │
│  ┌─────────────────────┐         ┌─────────────────────────┐    │
│  │ • Unreachable memory│         │ • Reachable but unused  │    │
│  │ • Double-free       │         │ • Growing unboundedly   │    │
│  │ • Use-after-free    │         │ • Resource leaks        │    │
│  └─────────────────────┘         └─────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 有垃圾回收，但仍然可能发生内存泄漏。GC 只能回收不可达的内存，如果你持有不再需要的引用（如无限增长的缓存），内存就会泄漏。此外，goroutine 泄漏、文件描述符泄漏等资源泄漏也很常见。

## 2. Core Idea and Mental Model

**A "leak" in Go = keeping references to memory you no longer need**

```
┌─────────────────────────────────────────────────────────────────┐
│                  Go Memory Leak Patterns                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Pattern 1: Unbounded Cache                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  var cache = make(map[string]*Data)                     │    │
│  │  func Store(k string, v *Data) { cache[k] = v }         │    │
│  │  // Never deletes → grows forever                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 2: Goroutine Leak                                       │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  go func() {                                            │    │
│  │      for data := range ch { }  // ch never closed       │    │
│  │  }()                                                    │    │
│  │  // Goroutine blocked forever, plus its stack          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 3: Slice Holding Backing Array                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  bigData := make([]byte, 1<<20)  // 1MB                 │    │
│  │  header := bigData[:10]          // 10 bytes, but...    │    │
│  │  return header                   // 1MB kept alive!     │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 4: Forgotten Timer                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  timer := time.AfterFunc(time.Hour, callback)           │    │
│  │  // If never stopped, timer + callback stay alive       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Detecting Leaks

```go
import (
    "runtime"
    "runtime/pprof"
)

// Monitor goroutine count
fmt.Println("Goroutines:", runtime.NumGoroutine())

// Heap profile
f, _ := os.Create("heap.prof")
pprof.WriteHeapProfile(f)
// Then: go tool pprof heap.prof

// Net/http profiler (add to server)
import _ "net/http/pprof"
// Visit: http://localhost:6060/debug/pprof/
```

### Common Leak Fixes

```go
// 1. Bounded cache with eviction
type LRUCache struct {
    maxSize int
    items   *list.List
    lookup  map[string]*list.Element
}

// 2. Goroutine with context cancellation
func worker(ctx context.Context, ch <-chan Work) {
    for {
        select {
        case w := <-ch:
            process(w)
        case <-ctx.Done():
            return  // Exit goroutine
        }
    }
}

// 3. Copy slice to release backing array
func getHeader(data []byte) []byte {
    header := make([]byte, 10)
    copy(header, data[:10])
    return header  // Only 10 bytes referenced
}

// 4. Stop timers
timer := time.NewTimer(time.Hour)
defer timer.Stop()
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
// Potential leak: Maps grow unbounded
var RouterAddresses map[uint32]map[int]RouterAddress
var VmcAddresses map[uint32]map[string]map[int]VmcAddress
var VmcRoutes map[string]map[VmcRoute]bool

// Mitigation: InformVmcDead cleans up
func (s *routermgrServer) InformVmcDead(context context.Context, vmcEvent *routermgrpb.VmcDeadEvent) (*routermgrpb.RouteActionResponse, error) {
    routeMutex.Lock()
    if vmcRoutes, vmcRoutesExists := VmcRoutes[vmcEvent.VmcName]; vmcRoutesExists {
        // Clean up routes for dead VMC
        for route, isV6 := range vmcRoutes {
            // Delete from FRR...
        }
        delete(VmcRoutes, vmcEvent.VmcName)  // Remove entry
    }
    routeMutex.Unlock()
    
    addressMutex.Lock()
    for vrfId, vrfVmcAddresses := range VmcAddresses {
        if vmcAddresses, vmcAddressesExists := vrfVmcAddresses[vmcEvent.VmcName]; vmcAddressesExists {
            // Clean up addresses...
            delete(VmcAddresses[vrfId], vmcEvent.VmcName)  // Remove entry
        }
    }
    addressMutex.Unlock()
    
    return &routermgrpb.RouteActionResponse{Success: true}, nil
}
```

**Good pattern**: The code cleans up when a VMC dies, preventing unbounded map growth.

### Goroutine Leak Example

```go
// Potential leak: HTTP handler spawns goroutine
func handleWebhook(w http.ResponseWriter, r *http.Request) {
    go func() {
        // If this blocks forever, goroutine leaks
        resp, err := http.Post(callbackURL, "application/json", body)
        if err != nil {
            return
        }
        resp.Body.Close()
    }()
    w.WriteHeader(http.StatusAccepted)
}

// Fix: Use context with timeout
func handleWebhook(w http.ResponseWriter, r *http.Request) {
    ctx, cancel := context.WithTimeout(r.Context(), 30*time.Second)
    
    go func() {
        defer cancel()
        req, _ := http.NewRequestWithContext(ctx, "POST", callbackURL, body)
        resp, err := http.DefaultClient.Do(req)
        if err != nil {
            return
        }
        resp.Body.Close()
    }()
    
    w.WriteHeader(http.StatusAccepted)
}
```

## 5. Common Mistakes and Pitfalls

1. **Unbounded slice/map growth**:
   ```go
   // Leak: history grows forever
   var history []Event
   func recordEvent(e Event) {
       history = append(history, e)
   }
   
   // Fix: bounded history
   const maxHistory = 1000
   func recordEvent(e Event) {
       if len(history) >= maxHistory {
           history = history[1:]  // Remove oldest
       }
       history = append(history, e)
   }
   ```

2. **Subslice of large slice**:
   ```go
   // Leak: keeps entire file in memory
   func readConfig(filename string) []byte {
       data, _ := ioutil.ReadFile(filename)  // 10MB file
       return data[:100]  // Only need 100 bytes, but 10MB retained
   }
   
   // Fix: copy needed portion
   func readConfig(filename string) []byte {
       data, _ := ioutil.ReadFile(filename)
       config := make([]byte, 100)
       copy(config, data[:100])
       return config  // Only 100 bytes retained
   }
   ```

3. **Goroutine blocked on channel**:
   ```go
   // Leak: goroutine never exits
   func process(jobs <-chan Job) {
       for job := range jobs {
           handle(job)
       }
       // If 'jobs' is never closed, goroutine leaks
   }
   
   // Fix: use context for cancellation
   func process(ctx context.Context, jobs <-chan Job) {
       for {
           select {
           case job := <-jobs:
               handle(job)
           case <-ctx.Done():
               return
           }
       }
   }
   ```

4. **Time.After in loop**:
   ```go
   // Leak: each iteration creates timer that won't be GC'd until fired
   for {
       select {
       case <-time.After(time.Second):
           doWork()
       case <-done:
           return
       }
   }
   
   // Fix: reuse timer
   timer := time.NewTimer(time.Second)
   defer timer.Stop()
   for {
       select {
       case <-timer.C:
           doWork()
           timer.Reset(time.Second)
       case <-done:
           return
       }
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Leak Type | C/C++ | Go |
|-----------|-------|-----|
| Forgot to free | Yes (classic leak) | No (GC prevents) |
| Unbounded cache | Yes | Yes |
| Resource leak (FD) | Yes | Yes |
| Thread/goroutine leak | Yes | Yes |
| Circular reference | Yes (some) | No (Go handles cycles) |

### C Memory Leak

```c
// C: classic leak - forgot to free
void process() {
    char* data = malloc(1000);
    // ... use data ...
    // Forgot: free(data);
}  // data leaked
```

### Go "Leak"

```go
// Go: reachable but unused - same effect
var cache = make(map[string][]byte)

func process(key string) {
    data := make([]byte, 1000)
    // ... use data ...
    cache[key] = data  // Now data is reachable forever
}
// GC can't reclaim because cache holds reference
```

## 7. A Small But Complete Go Example

```go
// leak_demo.go - Demonstrating common Go memory leaks
package main

import (
    "context"
    "fmt"
    "runtime"
    "time"
)

func printStats(label string) {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    fmt.Printf("%s - Goroutines: %d, Heap: %.2f MB\n",
        label, runtime.NumGoroutine(), float64(m.HeapAlloc)/1024/1024)
}

// ===== LEAK 1: Unbounded map =====
var leakyCache = make(map[int][]byte)

func demonstrateMapLeak() {
    fmt.Println("\n=== Map Leak ===")
    printStats("Before")
    
    for i := 0; i < 1000; i++ {
        leakyCache[i] = make([]byte, 10000)  // 10KB each
    }
    
    runtime.GC()
    printStats("After adding 1000 entries")
    
    // "Fix": clear the map
    for k := range leakyCache {
        delete(leakyCache, k)
    }
    
    runtime.GC()
    printStats("After cleanup")
}

// ===== LEAK 2: Goroutine leak =====
func leakyGoroutine(ch chan int) {
    <-ch  // Blocks forever if ch never receives
}

func demonstrateGoroutineLeak() {
    fmt.Println("\n=== Goroutine Leak ===")
    printStats("Before")
    
    for i := 0; i < 100; i++ {
        ch := make(chan int)
        go leakyGoroutine(ch)
        // Never send to ch, never close ch
        // Goroutine leaks!
    }
    
    runtime.GC()
    printStats("After spawning 100 leaky goroutines")
}

// Fixed version with context
func fixedGoroutine(ctx context.Context, ch chan int) {
    select {
    case <-ch:
        // Do work
    case <-ctx.Done():
        return  // Exit cleanly
    }
}

func demonstrateFixedGoroutine() {
    fmt.Println("\n=== Fixed Goroutine ===")
    initialCount := runtime.NumGoroutine()
    
    ctx, cancel := context.WithCancel(context.Background())
    
    for i := 0; i < 100; i++ {
        ch := make(chan int)
        go fixedGoroutine(ctx, ch)
    }
    
    printStats("After spawning 100 goroutines")
    
    cancel()  // Signal all goroutines to exit
    time.Sleep(100 * time.Millisecond)  // Let them exit
    
    printStats("After cancel")
    fmt.Printf("Goroutine increase: %d (should be ~0)\n",
        runtime.NumGoroutine()-initialCount)
}

// ===== LEAK 3: Slice retaining backing array =====
func demonstrateSliceLeak() {
    fmt.Println("\n=== Slice Backing Array Leak ===")
    
    // Simulate reading a large file
    largeData := make([]byte, 10*1024*1024)  // 10MB
    for i := range largeData {
        largeData[i] = byte(i)
    }
    
    // Leaky: keeps entire 10MB
    leakyHeader := largeData[:100]
    
    // Fixed: copies only needed data
    fixedHeader := make([]byte, 100)
    copy(fixedHeader, largeData[:100])
    
    // Clear original to see the difference
    largeData = nil
    runtime.GC()
    
    _ = leakyHeader  // Keeps 10MB alive
    _ = fixedHeader  // Only 100 bytes
    
    printStats("leakyHeader still references 10MB backing array")
}

func main() {
    printStats("Start")
    
    demonstrateMapLeak()
    demonstrateGoroutineLeak()
    demonstrateFixedGoroutine()
    demonstrateSliceLeak()
    
    fmt.Println("\n=== Summary ===")
    fmt.Println("Memory leaks in Go happen when you keep references")
    fmt.Println("to memory you no longer need. GC can't help if")
    fmt.Println("the memory is still reachable.")
    printStats("Final")
}
```

Output (approximate):
```
Start - Goroutines: 1, Heap: 0.07 MB

=== Map Leak ===
Before - Goroutines: 1, Heap: 0.07 MB
After adding 1000 entries - Goroutines: 1, Heap: 9.77 MB
After cleanup - Goroutines: 1, Heap: 0.10 MB

=== Goroutine Leak ===
Before - Goroutines: 1, Heap: 0.10 MB
After spawning 100 leaky goroutines - Goroutines: 101, Heap: 0.35 MB

=== Fixed Goroutine ===
After spawning 100 goroutines - Goroutines: 201, Heap: 0.60 MB
After cancel - Goroutines: 101, Heap: 0.35 MB
Goroutine increase: 0 (should be ~0)

=== Slice Backing Array Leak ===
leakyHeader still references 10MB backing array - Goroutines: 101, Heap: 10.42 MB

=== Summary ===
Memory leaks in Go happen when you keep references
to memory you no longer need. GC can't help if
the memory is still reachable.
Final - Goroutines: 101, Heap: 10.42 MB
```

---

**Summary**: Go's GC prevents traditional memory leaks (forgotten frees), but you can still leak memory by keeping unnecessary references. Common patterns include unbounded caches, goroutine leaks, and holding slice backing arrays. Use bounded data structures, context for goroutine cancellation, copy for small subslices, and profiling tools (pprof) to detect leaks.

