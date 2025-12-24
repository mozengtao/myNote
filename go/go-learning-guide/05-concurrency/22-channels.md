# Topic 22: Channels (Unbuffered vs Buffered)

## 1. Problem It Solves (Engineering Motivation)

Concurrent programming challenges:
- **Shared memory**: Race conditions, locks, deadlocks
- **Coordination**: How do goroutines know when data is ready?
- **Communication**: How to pass data between goroutines safely?

Go's philosophy: **"Don't communicate by sharing memory; share memory by communicating."**

```
┌─────────────────────────────────────────────────────────────────┐
│                  Communication Methods                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Shared Memory (C/C++/Java):     Channels (Go):                  │
│                                                                  │
│  ┌─────────┐    ┌─────────┐     ┌─────────┐ ──► ┌─────────┐    │
│  │ Thread1 │    │ Thread2 │     │Goroutine│     │Goroutine│    │
│  └────┬────┘    └────┬────┘     │    1    │ ch  │    2    │    │
│       │              │          └─────────┘ ──► └─────────┘    │
│       ▼              ▼                                          │
│  ┌────────────────────────┐     Data flows through channel      │
│  │   Shared Variable      │     No shared state                 │
│  │   (protected by lock)  │     No explicit locks               │
│  └────────────────────────┘     Synchronization built-in        │
│                                                                  │
│  Bugs: races, deadlocks         Safe by design                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Channel 是 Go 中 goroutine 之间通信的主要方式。不同于共享内存加锁的模式，channel 提供了类型安全的数据传递，同时内置了同步机制。无缓冲 channel 提供同步点，有缓冲 channel 提供有限的队列。

## 2. Core Idea and Mental Model

**A channel is a typed conduit through which you send and receive values.**

```
┌─────────────────────────────────────────────────────────────────┐
│                  Unbuffered vs Buffered Channels                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Unbuffered (synchronous):                                       │
│  ch := make(chan int)                                           │
│                                                                  │
│  Sender                      Receiver                           │
│  ┌───────┐                   ┌───────┐                          │
│  │ ch<-1 │ ═══════╗          │ <-ch  │                          │
│  │ blocks│        ║          │ blocks│                          │
│  └───────┘        ║          └───────┘                          │
│                   ║                                              │
│           Handoff happens only when                              │
│           both are ready (rendezvous)                            │
│                                                                  │
│  Buffered (asynchronous up to capacity):                         │
│  ch := make(chan int, 3)                                        │
│                                                                  │
│  Sender          Buffer           Receiver                       │
│  ┌───────┐    ┌───┬───┬───┐    ┌───────┐                        │
│  │ ch<-1 │───►│ 1 │ 2 │   │───►│ <-ch  │                        │
│  │ async │    └───┴───┴───┘    │       │                        │
│  └───────┘    capacity: 3      └───────┘                        │
│                                                                  │
│  Sender blocks only when buffer full                             │
│  Receiver blocks only when buffer empty                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Channel Creation

```go
// Unbuffered channel
ch := make(chan int)

// Buffered channel with capacity 10
ch := make(chan int, 10)

// Typed channels
stringCh := make(chan string)
structCh := make(chan MyStruct)
ptrCh := make(chan *MyStruct)
```

### Send and Receive

```go
// Send
ch <- value

// Receive
value := <-ch

// Receive with ok (false if closed and empty)
value, ok := <-ch
if !ok {
    fmt.Println("Channel closed")
}
```

### Closing Channels

```go
close(ch)  // Only sender should close

// Range over channel (stops when closed)
for value := range ch {
    process(value)
}
```

### Directional Channels

```go
// Send-only channel
func producer(ch chan<- int) {
    ch <- 42
}

// Receive-only channel  
func consumer(ch <-chan int) {
    value := <-ch
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go` context - channel patterns:

```go
// gRPC internally uses channels for request/response coordination
// Your handlers don't see them, but they're there

// Pattern: Processing pipeline
func processRoutes(routes <-chan Route) <-chan Result {
    results := make(chan Result)
    go func() {
        defer close(results)
        for route := range routes {
            result := processRoute(route)
            results <- result
        }
    }()
    return results
}
```

### Worker Pool with Channels

```go
func StartWorkerPool(numWorkers int) (chan<- Job, <-chan Result) {
    jobs := make(chan Job, 100)
    results := make(chan Result, 100)
    
    for i := 0; i < numWorkers; i++ {
        go worker(jobs, results)
    }
    
    return jobs, results
}

func worker(jobs <-chan Job, results chan<- Result) {
    for job := range jobs {
        results <- process(job)
    }
}
```

### Fan-out, Fan-in

```go
// Fan-out: distribute work to multiple goroutines
func fanOut(input <-chan int, numWorkers int) []<-chan int {
    outputs := make([]<-chan int, numWorkers)
    for i := 0; i < numWorkers; i++ {
        outputs[i] = worker(input)
    }
    return outputs
}

// Fan-in: merge multiple channels into one
func fanIn(inputs ...<-chan int) <-chan int {
    output := make(chan int)
    var wg sync.WaitGroup
    
    for _, in := range inputs {
        wg.Add(1)
        go func(ch <-chan int) {
            defer wg.Done()
            for v := range ch {
                output <- v
            }
        }(in)
    }
    
    go func() {
        wg.Wait()
        close(output)
    }()
    
    return output
}
```

## 5. Common Mistakes and Pitfalls

1. **Sending on closed channel (panic)**:
   ```go
   ch := make(chan int)
   close(ch)
   ch <- 1  // PANIC: send on closed channel
   
   // Rule: only sender closes, receiver checks ok
   ```

2. **Closing channel twice (panic)**:
   ```go
   close(ch)
   close(ch)  // PANIC: close of closed channel
   ```

3. **Deadlock - unbuffered without receiver**:
   ```go
   ch := make(chan int)
   ch <- 1  // DEADLOCK: no receiver
   
   // Fix: start receiver first or use buffered
   ```

4. **Goroutine leak - blocked receive**:
   ```go
   func process() <-chan int {
       ch := make(chan int)
       go func() {
           result := longComputation()
           ch <- result  // Blocks if no one receives
       }()
       return ch
   }
   
   // If caller ignores returned channel, goroutine leaks
   // Fix: use context for cancellation
   ```

5. **Not closing channels in range loops**:
   ```go
   // BAD: loop never exits
   go func() {
       for i := 0; i < 10; i++ {
           ch <- i
       }
       // Forgot close(ch)
   }()
   
   for v := range ch {  // Never exits
       fmt.Println(v)
   }
   
   // GOOD:
   go func() {
       defer close(ch)
       for i := 0; i < 10; i++ {
           ch <- i
       }
   }()
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C/pthreads | Go Channels |
|--------|------------|-------------|
| Communication | Shared memory + mutex | Channel send/receive |
| Synchronization | pthread_cond_wait | Implicit in channel |
| Type safety | None (void*) | Full |
| Buffering | Implement yourself | Built-in |
| Close/done | Manual signaling | close() + ok check |

### C/pthreads Producer-Consumer

```c
pthread_mutex_t mutex;
pthread_cond_t cond;
int data = 0;
int ready = 0;

void* producer(void* arg) {
    pthread_mutex_lock(&mutex);
    data = 42;
    ready = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* consumer(void* arg) {
    pthread_mutex_lock(&mutex);
    while (!ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    printf("Got: %d\n", data);
    pthread_mutex_unlock(&mutex);
    return NULL;
}
```

### Go Equivalent

```go
func producer(ch chan<- int) {
    ch <- 42
}

func consumer(ch <-chan int) {
    data := <-ch
    fmt.Println("Got:", data)
}

func main() {
    ch := make(chan int)
    go producer(ch)
    consumer(ch)
}
```

## 7. A Small But Complete Go Example

```go
// channels.go - Demonstrating channel patterns
package main

import (
    "fmt"
    "time"
)

func main() {
    // Unbuffered channel
    fmt.Println("=== Unbuffered Channel ===")
    unbuffered := make(chan string)
    
    go func() {
        unbuffered <- "hello"
        fmt.Println("Sent: hello")
    }()
    
    time.Sleep(100 * time.Millisecond)
    msg := <-unbuffered
    fmt.Println("Received:", msg)
    
    // Buffered channel
    fmt.Println("\n=== Buffered Channel ===")
    buffered := make(chan int, 3)
    
    buffered <- 1
    buffered <- 2
    buffered <- 3
    fmt.Println("Sent 3 values without blocking")
    
    fmt.Println("Received:", <-buffered, <-buffered, <-buffered)
    
    // Channel closing and range
    fmt.Println("\n=== Channel Range ===")
    numbers := make(chan int)
    
    go func() {
        for i := 1; i <= 5; i++ {
            numbers <- i
        }
        close(numbers)  // Signal no more values
    }()
    
    for n := range numbers {
        fmt.Printf("Got: %d\n", n)
    }
    fmt.Println("Channel closed, range done")
    
    // Checking if closed
    fmt.Println("\n=== Checking Closed ===")
    ch := make(chan int, 1)
    ch <- 42
    close(ch)
    
    v1, ok1 := <-ch
    fmt.Printf("First receive: %d, ok=%v\n", v1, ok1)
    
    v2, ok2 := <-ch
    fmt.Printf("Second receive: %d, ok=%v\n", v2, ok2)
    
    // Pipeline pattern
    fmt.Println("\n=== Pipeline Pattern ===")
    
    // Stage 1: Generate numbers
    gen := func(nums ...int) <-chan int {
        out := make(chan int)
        go func() {
            defer close(out)
            for _, n := range nums {
                out <- n
            }
        }()
        return out
    }
    
    // Stage 2: Square numbers
    sq := func(in <-chan int) <-chan int {
        out := make(chan int)
        go func() {
            defer close(out)
            for n := range in {
                out <- n * n
            }
        }()
        return out
    }
    
    // Connect pipeline
    numbers2 := gen(1, 2, 3, 4, 5)
    squares := sq(numbers2)
    
    for result := range squares {
        fmt.Printf("Squared: %d\n", result)
    }
    
    // Directional channels
    fmt.Println("\n=== Directional Channels ===")
    ping := make(chan string, 1)
    pong := make(chan string, 1)
    
    // Send-only
    sendOnly := func(ch chan<- string, msg string) {
        ch <- msg
    }
    
    // Receive-only
    receiveOnly := func(ch <-chan string) string {
        return <-ch
    }
    
    sendOnly(ping, "PING")
    fmt.Println("ping:", receiveOnly(ping))
}
```

Output:
```
=== Unbuffered Channel ===
Received: hello
Sent: hello

=== Buffered Channel ===
Sent 3 values without blocking
Received: 1 2 3

=== Channel Range ===
Got: 1
Got: 2
Got: 3
Got: 4
Got: 5
Channel closed, range done

=== Checking Closed ===
First receive: 42, ok=true
Second receive: 0, ok=false

=== Pipeline Pattern ===
Squared: 1
Squared: 4
Squared: 9
Squared: 16
Squared: 25

=== Directional Channels ===
ping: PING
```

---

**Summary**: Channels are Go's primary mechanism for goroutine communication. Unbuffered channels synchronize sender and receiver; buffered channels allow asynchronous sends up to capacity. Use `close()` to signal completion, `range` to receive until closed, and directional types for API clarity. Channels replace much of the mutex/condition variable complexity found in traditional threading.

