# Topic 21: Goroutines vs OS Threads

## 1. Problem It Solves (Engineering Motivation)

OS threads are expensive:
- **Memory**: ~1-8MB stack per thread
- **Creation**: ~1ms to spawn a thread
- **Context switching**: ~1-10μs (kernel involvement)
- **Scalability**: Thousands of threads = system strain

```
┌─────────────────────────────────────────────────────────────────┐
│                 Thread Model Comparison                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  OS Threads:                      Goroutines:                    │
│  ┌───────────────────────┐       ┌───────────────────────┐      │
│  │ 1MB+ stack each      │       │ 2KB stack (grows)     │      │
│  │ Kernel scheduled     │       │ User-space scheduled  │      │
│  │ 1ms to create        │       │ ~1μs to create        │      │
│  │ 10K threads = strain │       │ 1M goroutines = OK    │      │
│  └───────────────────────┘       └───────────────────────┘      │
│                                                                  │
│  OS Thread Model:                Goroutine Model (M:N):          │
│  ┌─────┐ ┌─────┐ ┌─────┐       ┌───┬───┬───┬───┬───┬───┐       │
│  │ T1  │ │ T2  │ │ T3  │       │G1 │G2 │G3 │G4 │G5 │G6 │ ...   │
│  └──┬──┘ └──┬──┘ └──┬──┘       └─┬─┴─┬─┴─┬─┴───┴───┴───┘       │
│     │       │       │            │   │   │                      │
│  ┌──┴───────┴───────┴──┐       ┌─┴───┴───┴─┐                    │
│  │      OS Kernel      │       │ Go Runtime │ (schedules)       │
│  └─────────────────────┘       └─────┬──────┘                    │
│                                      │                           │
│  1:1 mapping                   ┌─────┴─────┐                    │
│                                │ OS Threads │ (fewer)           │
│                                └────────────┘                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Goroutine 是 Go 的轻量级线程。与 OS 线程相比，goroutine 栈初始只有 2KB（可以增长），创建只需约 1 微秒，可以轻松运行数十万个。Go 运行时将大量 goroutine 调度到少量 OS 线程上（M:N 调度），这是 Go 高并发性能的关键。

## 2. Core Idea and Mental Model

**Goroutines are functions that run concurrently.**

The Go runtime implements a scheduler (M:N scheduling):
- **G**: Goroutines (lightweight)
- **M**: OS threads (Machine)  
- **P**: Processors (logical CPUs, = GOMAXPROCS)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Go Scheduler (GMP Model)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Goroutines (G):       Many, lightweight                        │
│  ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐                    │
│  │G1 │ │G2 │ │G3 │ │G4 │ │G5 │ │G6 │ │G7 │ ...                │
│  └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘                    │
│    │     │     │     │     │     │     │                        │
│    └─────┴─────┼─────┴─────┼─────┴─────┘                        │
│                │           │                                     │
│  Processors (P): GOMAXPROCS (usually = num CPUs)                │
│         ┌──────┴─────┐  ┌──┴─────┐                              │
│         │    P1      │  │   P2   │                              │
│         │ (run queue)│  │        │                              │
│         └──────┬─────┘  └────┬───┘                              │
│                │             │                                   │
│  OS Threads (M): Few, managed by runtime                        │
│         ┌──────┴─────┐  ┌────┴───┐                              │
│         │    M1      │  │   M2   │                              │
│         └──────┬─────┘  └────┬───┘                              │
│                │             │                                   │
│         ┌──────┴─────────────┴───┐                              │
│         │      OS Kernel         │                              │
│         └────────────────────────┘                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Creating Goroutines

```go
// Start a goroutine
go myFunction()

// Goroutine with anonymous function
go func() {
    // Do work...
}()

// Goroutine with parameters
go func(msg string) {
    fmt.Println(msg)
}("hello")
```

### Waiting for Goroutines

```go
import "sync"

var wg sync.WaitGroup

wg.Add(1)  // Increment counter
go func() {
    defer wg.Done()  // Decrement counter when done
    // Do work...
}()

wg.Wait()  // Block until counter is zero
```

### Runtime Control

```go
import "runtime"

// Number of logical CPUs
n := runtime.NumCPU()

// Number of goroutines
n := runtime.NumGoroutine()

// Set max parallel OS threads for Go code
runtime.GOMAXPROCS(4)

// Yield to other goroutines
runtime.Gosched()
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go` context:

```go
func StartGrpcServer() {
    // gRPC server internally uses goroutines
    // Each incoming request is handled in a separate goroutine
    grpcServer := grpc.NewServer()
    routermgrpb.RegisterRouterMgrServer(grpcServer, &routermgrSrv)
    grpcServer.Serve(lis)  // Spawns goroutine per connection
}

// Each RPC method runs in its own goroutine
func (s *routermgrServer) AddRouteV4(ctx context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // This runs concurrently with other requests
    // Multiple AddRouteV4 calls can execute simultaneously
    routeMutex.Lock()  // Protect shared state
    defer routeMutex.Unlock()
    // ...
}
```

### Concurrent Server Pattern

```go
func main() {
    listener, _ := net.Listen("tcp", ":8080")
    
    for {
        conn, _ := listener.Accept()
        go handleConnection(conn)  // New goroutine per connection
    }
}

func handleConnection(conn net.Conn) {
    defer conn.Close()
    // Handle this connection...
}
```

### Worker Pool Pattern

```go
func worker(id int, jobs <-chan int, results chan<- int) {
    for j := range jobs {
        results <- process(j)
    }
}

func main() {
    jobs := make(chan int, 100)
    results := make(chan int, 100)
    
    // Start 3 workers
    for w := 1; w <= 3; w++ {
        go worker(w, jobs, results)
    }
    
    // Send jobs
    for j := 1; j <= 9; j++ {
        jobs <- j
    }
    close(jobs)
    
    // Collect results
    for a := 1; a <= 9; a++ {
        <-results
    }
}
```

## 5. Common Mistakes and Pitfalls

1. **Goroutine leak**:
   ```go
   // BAD: goroutine blocks forever
   go func() {
       data := <-ch  // What if ch never sends?
   }()
   
   // GOOD: use context for cancellation
   go func() {
       select {
       case data := <-ch:
           process(data)
       case <-ctx.Done():
           return
       }
   }()
   ```

2. **Race condition on loop variable**:
   ```go
   // BAD: all goroutines see final value of i
   for i := 0; i < 10; i++ {
       go func() {
           fmt.Println(i)  // All print 10!
       }()
   }
   
   // GOOD: pass as parameter
   for i := 0; i < 10; i++ {
       go func(n int) {
           fmt.Println(n)
       }(i)
   }
   
   // GOOD (Go 1.22+): loop variables are per-iteration
   for i := 0; i < 10; i++ {
       go func() {
           fmt.Println(i)  // Works correctly in Go 1.22+
       }()
   }
   ```

3. **No synchronization**:
   ```go
   // BAD: data race
   counter := 0
   for i := 0; i < 1000; i++ {
       go func() { counter++ }()  // Race!
   }
   
   // GOOD: use atomic or mutex
   var counter int64
   for i := 0; i < 1000; i++ {
       go func() {
           atomic.AddInt64(&counter, 1)
       }()
   }
   ```

4. **Forgetting WaitGroup**:
   ```go
   // BAD: main exits before goroutines complete
   for i := 0; i < 10; i++ {
       go doWork(i)
   }
   // Program may exit immediately!
   
   // GOOD: wait for completion
   var wg sync.WaitGroup
   for i := 0; i < 10; i++ {
       wg.Add(1)
       go func(n int) {
           defer wg.Done()
           doWork(n)
       }(i)
   }
   wg.Wait()
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | pthreads (C/C++) | Go Goroutines |
|--------|------------------|---------------|
| Creation | `pthread_create()` | `go f()` |
| Stack size | 1-8MB fixed | 2KB, grows |
| Create time | ~1ms | ~1μs |
| Max practical | ~10K | ~1M |
| Scheduling | OS kernel | Go runtime |
| Communication | Shared memory + locks | Channels (preferred) |

### C pthreads

```c
void* worker(void* arg) {
    int id = *(int*)arg;
    printf("Worker %d\n", id);
    return NULL;
}

int main() {
    pthread_t threads[10];
    int ids[10];
    
    for (int i = 0; i < 10; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }
    
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }
}
```

### Go Equivalent

```go
func main() {
    var wg sync.WaitGroup
    
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            fmt.Printf("Worker %d\n", id)
        }(i)
    }
    
    wg.Wait()
}
```

## 7. A Small But Complete Go Example

```go
// goroutines.go - Demonstrating goroutines
package main

import (
    "fmt"
    "runtime"
    "sync"
    "time"
)

func main() {
    fmt.Printf("CPUs: %d, GOMAXPROCS: %d\n\n", 
        runtime.NumCPU(), runtime.GOMAXPROCS(0))
    
    // Basic goroutine
    fmt.Println("=== Basic Goroutine ===")
    go func() {
        fmt.Println("Hello from goroutine!")
    }()
    time.Sleep(10 * time.Millisecond)
    
    // WaitGroup pattern
    fmt.Println("\n=== WaitGroup Pattern ===")
    var wg sync.WaitGroup
    
    for i := 1; i <= 3; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            fmt.Printf("Worker %d starting\n", id)
            time.Sleep(100 * time.Millisecond)
            fmt.Printf("Worker %d done\n", id)
        }(i)
    }
    
    wg.Wait()
    fmt.Println("All workers completed")
    
    // Many goroutines
    fmt.Println("\n=== Many Goroutines ===")
    before := runtime.NumGoroutine()
    
    var wg2 sync.WaitGroup
    for i := 0; i < 10000; i++ {
        wg2.Add(1)
        go func() {
            defer wg2.Done()
            time.Sleep(10 * time.Millisecond)
        }()
    }
    
    peak := runtime.NumGoroutine()
    wg2.Wait()
    after := runtime.NumGoroutine()
    
    fmt.Printf("Before: %d, Peak: %d, After: %d\n", before, peak, after)
    
    // Goroutine communication via channel
    fmt.Println("\n=== Channel Communication ===")
    results := make(chan int, 5)
    
    for i := 1; i <= 5; i++ {
        go func(n int) {
            results <- n * n
        }(i)
    }
    
    for i := 0; i < 5; i++ {
        fmt.Printf("Result: %d\n", <-results)
    }
}
```

Output:
```
CPUs: 8, GOMAXPROCS: 8

=== Basic Goroutine ===
Hello from goroutine!

=== WaitGroup Pattern ===
Worker 3 starting
Worker 1 starting
Worker 2 starting
Worker 1 done
Worker 3 done
Worker 2 done
All workers completed

=== Many Goroutines ===
Before: 1, Peak: 10001, After: 1

=== Channel Communication ===
Result: 1
Result: 4
Result: 9
Result: 16
Result: 25
```

---

**Summary**: Goroutines are Go's lightweight concurrency primitive. They're cheap (2KB initial stack, ~1μs to create) and managed by Go's runtime scheduler, not the OS kernel. This enables millions of concurrent goroutines on modest hardware. Use `go` keyword to start, `sync.WaitGroup` to wait, and channels to communicate. Be careful of goroutine leaks and data races.

