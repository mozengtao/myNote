# Channels: Go's Concurrency Primitive

## 1. Engineering Problem

### What real-world problem does this solve?

**The fundamental challenge: How do concurrent tasks communicate safely?**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 CONCURRENT COMMUNICATION APPROACHES                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Shared Memory + Locks (C/C++/Java)                                   │
│   ──────────────────────────────────                                    │
│                                                                         │
│   Thread A ─────┐        ┌───────────────┐        ┌───── Thread B       │
│                 │        │ Shared Memory │        │                     │
│          lock() ├───────►│   (data)      │◄───────┤ lock()              │
│         unlock()│        │               │        │unlock()             │
│                 │        └───────────────┘        │                     │
│                                                                         │
│   Problems:                                                             │
│   • Deadlocks (A waits for B, B waits for A)                           │
│   • Priority inversion                                                  │
│   • Forgotten locks → data races                                        │
│   • Lock contention → performance collapse                              │
│   • Hard to reason about correctness                                    │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Message Passing (Go Channels, Erlang, Actor Model)                    │
│   ──────────────────────────────────────────────────                    │
│                                                                         │
│   Goroutine A ────►[ channel ]────► Goroutine B                         │
│                                                                         │
│   Benefits:                                                             │
│   • Data moves, not shared (ownership transfer)                         │
│   • No explicit locks in user code                                      │
│   • Synchronization is implicit                                         │
│   • Easier to reason about (data flows in one direction)                │
│   • Deadlocks become channel deadlocks (easier to debug)                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Go's philosophy: "Don't communicate by sharing memory; share memory by communicating."**

### What goes wrong when engineers misunderstand channels?

1. **Goroutine leaks**: Goroutines blocked forever on channel operations
2. **Deadlocks**: Circular wait on channels with no progress
3. **Premature closes**: Sending on closed channel = panic
4. **Wrong channel direction**: Using buffered when unbuffered needed (or vice versa)
5. **Overuse**: Using channels where a mutex would be simpler

---

## 2. Core Mental Model

### Channel as a typed conduit

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      CHANNEL MENTAL MODEL                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   A channel is:                                                         │
│   • A typed pipe for passing values between goroutines                  │
│   • A synchronization primitive (send/receive are sync points)          │
│   • A mechanism for ownership transfer                                  │
│                                                                         │
│   ┌─────────────┐                           ┌─────────────┐             │
│   │ Goroutine A │                           │ Goroutine B │             │
│   │             │   ch <- value             │             │             │
│   │   Sender    │──────────────►[  ch  ]───►│  Receiver   │             │
│   │             │                 │         │  v := <-ch  │             │
│   └─────────────┘                 │         └─────────────┘             │
│                                   │                                     │
│                       ┌───────────┴───────────┐                         │
│                       │ UNBUFFERED (len=0)    │                         │
│                       │ • Sender blocks until │                         │
│                       │   receiver takes      │                         │
│                       │ • Synchronous handoff │                         │
│                       │ • Guarantees delivery │                         │
│                       │   before send returns │                         │
│                       └───────────────────────┘                         │
│                                                                         │
│                       ┌───────────────────────┐                         │
│                       │ BUFFERED (len=N)      │                         │
│                       │ • Sender blocks only  │                         │
│                       │   when buffer full    │                         │
│                       │ • Async up to N items │                         │
│                       │ • No delivery         │                         │
│                       │   guarantee on send   │                         │
│                       └───────────────────────┘                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Unbuffered vs Buffered: The Critical Distinction

```
┌─────────────────────────────────────────────────────────────────────────┐
│              UNBUFFERED vs BUFFERED CHANNELS                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   UNBUFFERED: make(chan T)      BUFFERED: make(chan T, n)              │
│   ────────────────────────      ─────────────────────────               │
│                                                                         │
│   Sender ──send──► │ │ ◄──recv── Receiver                               │
│           blocks   │ │   blocks                                         │
│           until    │ │   until                                          │
│           recv     │ │   send                                           │
│                    │ │                                                  │
│   Synchronization: RENDEZVOUS                                           │
│   Both goroutines meet at the channel                                   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Sender ──send──► [x][x][x][_] ◄──recv── Receiver                      │
│           blocks   │  buffer  │   blocks                                │
│           when     │  cap=4   │   when                                  │
│           full     │  len=3   │   empty                                 │
│                                                                         │
│   Synchronization: DECOUPLED                                            │
│   Goroutines can proceed independently up to buffer size                │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   When to use each:                                                     │
│                                                                         │
│   Unbuffered:                  Buffered:                                │
│   • Synchronous handoff        • Producer-consumer patterns             │
│   • Request-response           • Bursty workloads                       │
│   • Signal completion          • Rate limiting (fixed buffer)           │
│   • Lock-step coordination     • "Fire and forget" with bound           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Language Mechanism

### Channel Operations

```go
// Creation
ch := make(chan int)       // Unbuffered
ch := make(chan int, 100)  // Buffered with capacity 100

// Send (blocks if channel full or unbuffered with no receiver)
ch <- value

// Receive (blocks if channel empty)
value := <-ch              // Receive and assign
value, ok := <-ch          // ok is false if channel closed and empty
<-ch                       // Receive and discard

// Close (only sender should close; panic if send to closed)
close(ch)

// Length and capacity
len(ch)                    // Number of elements currently in buffer
cap(ch)                    // Buffer capacity (0 for unbuffered)
```

### Channel States and Operations

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   CHANNEL STATE MACHINE                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   State        │ Send (ch<-v)    │ Receive (<-ch)  │ Close             │
│   ─────────────┼─────────────────┼─────────────────┼─────────────────  │
│   nil          │ block forever   │ block forever   │ panic             │
│   open, empty  │ block/succeed*  │ block           │ succeed           │
│   open, full   │ block           │ succeed         │ succeed           │
│   open, partial│ succeed         │ succeed         │ succeed           │
│   closed       │ PANIC           │ zero, ok=false  │ panic             │
│                                                                         │
│   *unbuffered blocks until receiver; buffered succeeds if not full      │
│                                                                         │
│   Critical rules:                                                       │
│   1. Only the sender should close a channel                             │
│   2. Never close a channel from receiver side                           │
│   3. Never send on a closed channel                                     │
│   4. Multiple receivers are fine; multiple senders need coordination    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Range over Channel

```go
// Idiomatic way to receive until channel closed
for value := range ch {
    process(value)
}
// Exits when ch is closed AND drained
```

### Directional Channels (Type Safety)

```go
// Send-only channel
func producer(out chan<- int) {
    out <- 42
    // <-out  // Compile error: cannot receive
}

// Receive-only channel
func consumer(in <-chan int) {
    v := <-in
    // in <- 42  // Compile error: cannot send
}

// Bidirectional can be assigned to directional
ch := make(chan int)
producer(ch)  // OK: chan int → chan<- int
consumer(ch)  // OK: chan int → <-chan int
```

---

## 4. Idiomatic Usage

### Pattern 1: Done Channel (Signaling completion)

```go
func worker(done chan struct{}) {
    defer close(done)  // Signal completion when function exits
    
    // Do work...
}

func main() {
    done := make(chan struct{})
    go worker(done)
    
    <-done  // Wait for worker to complete
}
```

### Pattern 2: Generator

```go
func generateNumbers(ctx context.Context) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for i := 0; ; i++ {
            select {
            case <-ctx.Done():
                return
            case out <- i:
            }
        }
    }()
    return out
}
```

### Pattern 3: Fan-out / Fan-in

```go
// Fan-out: distribute work to multiple workers
func fanOut(input <-chan Job, workers int) []<-chan Result {
    outputs := make([]<-chan Result, workers)
    for i := 0; i < workers; i++ {
        outputs[i] = worker(input)
    }
    return outputs
}

// Fan-in: merge multiple channels into one
func fanIn(inputs ...<-chan Result) <-chan Result {
    out := make(chan Result)
    var wg sync.WaitGroup
    
    for _, in := range inputs {
        wg.Add(1)
        go func(ch <-chan Result) {
            defer wg.Done()
            for v := range ch {
                out <- v
            }
        }(in)
    }
    
    go func() {
        wg.Wait()
        close(out)
    }()
    
    return out
}
```

### Pattern 4: Semaphore (Bounded Parallelism)

```go
// Limit concurrent operations using a buffered channel
sem := make(chan struct{}, 10)  // Max 10 concurrent

for _, item := range items {
    sem <- struct{}{}  // Acquire
    go func(item Item) {
        defer func() { <-sem }()  // Release
        process(item)
    }(item)
}
```

### Pattern 5: Timeout with channel

```go
select {
case result := <-resultCh:
    process(result)
case <-time.After(5 * time.Second):
    return errors.New("timeout")
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Goroutine leak from blocked channel

```go
// LEAK: If consumer stops reading, producer blocks forever
func leaky() {
    ch := make(chan int)
    go func() {
        for i := 0; i < 100; i++ {
            ch <- i  // Blocks if no receiver
        }
    }()
    
    // Only read 5 items
    for i := 0; i < 5; i++ {
        <-ch
    }
    // Goroutine is now leaked, waiting on ch <- 5
}

// FIX: Use context for cancellation
func notLeaky(ctx context.Context) {
    ch := make(chan int)
    go func() {
        defer close(ch)
        for i := 0; i < 100; i++ {
            select {
            case <-ctx.Done():
                return
            case ch <- i:
            }
        }
    }()
    
    for i := 0; i < 5; i++ {
        select {
        case <-ctx.Done():
            return
        case v := <-ch:
            process(v)
        }
    }
    // Context cancellation will clean up goroutine
}
```

### Pitfall 2: Sending on closed channel

```go
// PANIC: Sending on closed channel
func badClose() {
    ch := make(chan int)
    close(ch)
    ch <- 1  // panic: send on closed channel
}

// RULE: Only sender closes. Use done channel for signaling.
```

### Pitfall 3: Closing from wrong goroutine

```go
// BAD: Multiple senders, one might close while others send
func multipleSenders(ch chan int) {
    go func() { ch <- 1; close(ch) }()  // Close here
    go func() { ch <- 2 }()              // May panic!
}

// FIX: Use sync.Once or have single closer
func multipleSendersSafe(ch chan int) {
    var wg sync.WaitGroup
    wg.Add(2)
    
    go func() { defer wg.Done(); ch <- 1 }()
    go func() { defer wg.Done(); ch <- 2 }()
    
    go func() {
        wg.Wait()
        close(ch)  // Close only after all senders done
    }()
}
```

### Pitfall 4: Using unbuffered when buffered needed

```go
// BAD: Unbuffered channel for async notification
func notify() chan struct{} {
    ch := make(chan struct{})
    go func() {
        doWork()
        ch <- struct{}{}  // Blocks if no receiver yet!
    }()
    return ch
}

// FIX: Buffered channel of 1 for async notification
func notify() chan struct{} {
    ch := make(chan struct{}, 1)
    go func() {
        doWork()
        ch <- struct{}{}  // Never blocks
    }()
    return ch
}
```

### Pitfall 5: nil channel operations

```go
// Block forever on nil channel
var ch chan int  // nil
ch <- 1          // Blocks forever
<-ch             // Blocks forever

// This is actually useful in select to disable a case:
var chA, chB chan int
if someCondition {
    chA = actualChannel
}
// chB is nil, so its case is never selected
select {
case v := <-chA:  // Selected if chA != nil
case v := <-chB:  // Never selected, chB is nil
}
```

### Pitfall 6: Forgetting that range loops block

```go
// DEADLOCK: range waits for close, close never happens
func deadlock() {
    ch := make(chan int)
    go func() {
        for v := range ch {  // Waits for close
            fmt.Println(v)
        }
    }()
    
    ch <- 1
    ch <- 2
    // Forgot close(ch)
    // Main exits, but goroutine leaks
}
```

---

## 6. Complete, Realistic Example

This example shows a production-grade pipeline with proper lifecycle management:

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "sync"
    "time"
)

// RouteUpdate represents a routing table change (from routermgr context)
type RouteUpdate struct {
    VrfID     uint32
    Prefix    string
    NextHop   string
    Operation string // "add", "delete"
}

// ProcessedRoute is the result of processing a RouteUpdate
type ProcessedRoute struct {
    Update    RouteUpdate
    Validated bool
    Applied   bool
    Error     error
}

// Pipeline demonstrates a multi-stage processing pipeline using channels
type Pipeline struct {
    // Input channel - where raw updates come in
    input chan RouteUpdate
    
    // Stage output channels
    validated chan RouteUpdate
    applied   chan ProcessedRoute
    
    // Control
    ctx    context.Context
    cancel context.CancelFunc
    wg     sync.WaitGroup
    
    // Configuration
    validators int
    appliers   int
}

func NewPipeline(inputBuffer, validators, appliers int) *Pipeline {
    ctx, cancel := context.WithCancel(context.Background())
    
    return &Pipeline{
        input:      make(chan RouteUpdate, inputBuffer),
        validated:  make(chan RouteUpdate, validators),
        applied:    make(chan ProcessedRoute, appliers),
        ctx:        ctx,
        cancel:     cancel,
        validators: validators,
        appliers:   appliers,
    }
}

// Start launches all pipeline goroutines
func (p *Pipeline) Start() {
    log.Printf("Starting pipeline: validators=%d, appliers=%d",
        p.validators, p.appliers)
    
    // Stage 1: Validators (fan-out from input)
    for i := 0; i < p.validators; i++ {
        p.wg.Add(1)
        go p.validator(i)
    }
    
    // Stage 2: Appliers (fan-out from validated)
    for i := 0; i < p.appliers; i++ {
        p.wg.Add(1)
        go p.applier(i)
    }
    
    // Collector goroutine for final results
    p.wg.Add(1)
    go p.collector()
    
    log.Println("Pipeline started")
}

// validator validates route updates
func (p *Pipeline) validator(id int) {
    defer p.wg.Done()
    
    for {
        select {
        case <-p.ctx.Done():
            log.Printf("Validator %d: shutting down", id)
            return
            
        case update, ok := <-p.input:
            if !ok {
                log.Printf("Validator %d: input closed", id)
                return
            }
            
            // Simulate validation
            if err := p.validateRoute(update); err != nil {
                log.Printf("Validator %d: invalid route %s: %v",
                    id, update.Prefix, err)
                continue  // Skip invalid routes
            }
            
            // Forward to next stage
            select {
            case <-p.ctx.Done():
                return
            case p.validated <- update:
            }
        }
    }
}

// validateRoute performs route validation
func (p *Pipeline) validateRoute(update RouteUpdate) error {
    // Simulate validation logic
    if update.Prefix == "" {
        return errors.New("empty prefix")
    }
    if update.Operation != "add" && update.Operation != "delete" {
        return fmt.Errorf("invalid operation: %s", update.Operation)
    }
    
    time.Sleep(5 * time.Millisecond)  // Simulate work
    return nil
}

// applier applies validated routes to the system
func (p *Pipeline) applier(id int) {
    defer p.wg.Done()
    
    for {
        select {
        case <-p.ctx.Done():
            log.Printf("Applier %d: shutting down", id)
            return
            
        case update, ok := <-p.validated:
            if !ok {
                log.Printf("Applier %d: validated closed", id)
                return
            }
            
            result := ProcessedRoute{
                Update:    update,
                Validated: true,
            }
            
            // Simulate applying route
            if err := p.applyRoute(update); err != nil {
                result.Error = err
            } else {
                result.Applied = true
            }
            
            // Forward result
            select {
            case <-p.ctx.Done():
                return
            case p.applied <- result:
            }
        }
    }
}

// applyRoute applies a route to the system
func (p *Pipeline) applyRoute(update RouteUpdate) error {
    // Simulate system call
    time.Sleep(10 * time.Millisecond)
    
    log.Printf("Applied: %s %s via %s (VRF %d)",
        update.Operation, update.Prefix, update.NextHop, update.VrfID)
    
    return nil
}

// collector collects final results
func (p *Pipeline) collector() {
    defer p.wg.Done()
    
    var success, failed int
    
    for {
        select {
        case <-p.ctx.Done():
            log.Printf("Collector: shutting down (success=%d, failed=%d)",
                success, failed)
            return
            
        case result, ok := <-p.applied:
            if !ok {
                log.Printf("Collector: applied closed (success=%d, failed=%d)",
                    success, failed)
                return
            }
            
            if result.Error != nil {
                failed++
                log.Printf("Failed to apply %s: %v",
                    result.Update.Prefix, result.Error)
            } else {
                success++
            }
        }
    }
}

// Submit adds a route update to the pipeline
func (p *Pipeline) Submit(ctx context.Context, update RouteUpdate) error {
    select {
    case <-ctx.Done():
        return ctx.Err()
    case <-p.ctx.Done():
        return errors.New("pipeline shutdown")
    case p.input <- update:
        return nil
    }
}

// Shutdown gracefully shuts down the pipeline
func (p *Pipeline) Shutdown(timeout time.Duration) error {
    log.Println("Shutting down pipeline...")
    
    // Close input channel to signal no more work
    close(p.input)
    
    // Give workers time to drain
    time.Sleep(100 * time.Millisecond)
    
    // Close intermediate channels
    close(p.validated)
    
    // Wait a bit more
    time.Sleep(100 * time.Millisecond)
    
    // Cancel context to stop all goroutines
    p.cancel()
    close(p.applied)
    
    // Wait for all goroutines
    done := make(chan struct{})
    go func() {
        p.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("Pipeline shutdown complete")
        return nil
    case <-time.After(timeout):
        return errors.New("shutdown timeout")
    }
}

// Demonstrate buffered channel as rate limiter
func demonstrateRateLimiter() {
    fmt.Println("\n=== Rate Limiter Demo ===")
    
    // Allow 3 requests per second
    limiter := make(chan struct{}, 3)
    
    // Fill the limiter
    for i := 0; i < 3; i++ {
        limiter <- struct{}{}
    }
    
    // Refill one token per 333ms
    go func() {
        ticker := time.NewTicker(333 * time.Millisecond)
        defer ticker.Stop()
        for range ticker.C {
            select {
            case limiter <- struct{}{}:
            default:  // Don't block if full
            }
        }
    }()
    
    // Simulate requests
    for i := 0; i < 10; i++ {
        <-limiter  // Wait for token
        fmt.Printf("Request %d at %v\n", i, time.Now().Format("15:04:05.000"))
    }
}

// Demonstrate unbuffered channel for synchronization
func demonstrateSynchronization() {
    fmt.Println("\n=== Synchronization Demo ===")
    
    ready := make(chan struct{})  // Unbuffered!
    
    go func() {
        fmt.Println("Worker: preparing...")
        time.Sleep(100 * time.Millisecond)
        fmt.Println("Worker: ready!")
        ready <- struct{}{}  // Blocks until main receives
        fmt.Println("Worker: main acknowledged, continuing...")
    }()
    
    fmt.Println("Main: waiting for worker...")
    <-ready  // Blocks until worker sends
    fmt.Println("Main: worker is ready, proceeding...")
    
    time.Sleep(50 * time.Millisecond)  // Let worker finish
}

func main() {
    demonstrateSynchronization()
    demonstrateRateLimiter()
    
    fmt.Println("\n=== Pipeline Demo ===")
    
    // Create pipeline with bounded buffers
    pipeline := NewPipeline(
        100, // Input buffer
        2,   // Validators
        2,   // Appliers
    )
    
    pipeline.Start()
    
    // Submit some route updates
    ctx := context.Background()
    routes := []RouteUpdate{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1", Operation: "add"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2", Operation: "add"},
        {VrfID: 1, Prefix: "10.0.2.0/24", NextHop: "192.168.1.3", Operation: "delete"},
        {VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1", Operation: "add"},
        {VrfID: 1, Prefix: "", NextHop: "", Operation: "add"},  // Invalid
    }
    
    for _, route := range routes {
        if err := pipeline.Submit(ctx, route); err != nil {
            log.Printf("Failed to submit route: %v", err)
        }
    }
    
    // Let pipeline process
    time.Sleep(500 * time.Millisecond)
    
    // Shutdown
    if err := pipeline.Shutdown(5 * time.Second); err != nil {
        log.Printf("Shutdown error: %v", err)
    }
    
    fmt.Println("\n=== Demo Complete ===")
}
```

---

## 7. Design Takeaways

### Rules of Thumb

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CHANNEL DESIGN RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. OWNERSHIP: Who closes?                                             │
│      • Only the sender closes                                           │
│      • Multiple senders? Use sync.WaitGroup + goroutine to close        │
│      • Never close from receiver                                        │
│                                                                         │
│   2. BUFFERING: How much?                                               │
│      • Unbuffered (0): Synchronization required                         │
│      • Buffer of 1: Async notification                                  │
│      • Fixed N: Known throughput/latency tradeoff                       │
│      • Unbounded (use list): Memory leak risk!                          │
│                                                                         │
│   3. DIRECTION: Use directional types                                   │
│      • chan<- T for send-only (producer)                                │
│      • <-chan T for receive-only (consumer)                             │
│      • Documents intent, compiler enforces                              │
│                                                                         │
│   4. CANCELLATION: Always provide escape                                │
│      • Every send/receive in a select                                   │
│      • Include ctx.Done() case                                          │
│      • Buffered + non-blocking = fire-and-forget                        │
│                                                                         │
│   5. WHEN TO USE:                                                       │
│      • Passing data ownership between goroutines                        │
│      • Coordinating multiple goroutines                                 │
│      • Signaling events (done, ready)                                   │
│      • Pipeline processing                                              │
│                                                                         │
│   6. WHEN NOT TO USE:                                                   │
│      • Protecting shared state (use mutex)                              │
│      • Simple request-response (may be overkill)                        │
│      • When mutex is clearer                                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Architecture Implications

```
┌─────────────────────────────────────────────────────────────────────────┐
│                 CHANNEL-BASED ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Pipeline Pattern:                                                     │
│   ┌───────┐     ┌───────┐     ┌───────┐     ┌───────┐                  │
│   │ Input │────►│ Stage1│────►│ Stage2│────►│ Output│                  │
│   └───────┘     └───────┘     └───────┘     └───────┘                  │
│   (buffered)   (workers)     (workers)     (collector)                  │
│                                                                         │
│   Key decisions:                                                        │
│   • Buffer sizes = throughput × acceptable latency                      │
│   • Worker count = based on stage's I/O vs CPU characteristics          │
│   • Shutdown order = upstream to downstream                             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Fan-out / Fan-in:                                                     │
│                    ┌─────────┐                                          │
│                ┌──►│ Worker1 │───┐                                      │
│   ┌───────┐   │   └─────────┘   │   ┌───────┐                          │
│   │ Input │───┼──►│ Worker2 │───┼──►│ Merge │                          │
│   └───────┘   │   └─────────┘   │   └───────┘                          │
│                └──►│ Worker3 │───┘                                      │
│                    └─────────┘                                          │
│                                                                         │
│   Key decisions:                                                        │
│   • Worker count = min(GOMAXPROCS, expected parallelism benefit)        │
│   • Merge must handle partial results on shutdown                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Channel 核心概念

**Channel 是 Go 中 goroutine 之间通信的类型安全管道。**

核心原则：**"不要通过共享内存来通信，而要通过通信来共享内存。"**

### 无缓冲 vs 有缓冲

| 特性 | 无缓冲 `make(chan T)` | 有缓冲 `make(chan T, n)` |
|------|----------------------|-------------------------|
| 发送 | 阻塞直到有接收者 | 缓冲未满时不阻塞 |
| 接收 | 阻塞直到有发送者 | 缓冲为空时阻塞 |
| 同步 | 同步交接（rendezvous） | 解耦（最多 n 个元素） |
| 用途 | 同步、信号传递 | 生产者-消费者、限流 |

### Channel 状态表

| 状态 | 发送 | 接收 | 关闭 |
|------|------|------|------|
| nil | 永远阻塞 | 永远阻塞 | panic |
| 打开，空 | 阻塞/成功 | 阻塞 | 成功 |
| 打开，满 | 阻塞 | 成功 | 成功 |
| 已关闭 | **panic** | 返回零值，ok=false | panic |

### 关键规则

1. **只有发送者关闭 channel**
   - 永远不要从接收方关闭
   - 多个发送者时，用 WaitGroup 协调后关闭

2. **缓冲区大小设计**
   - 0（无缓冲）：需要同步
   - 1：异步通知
   - N：吞吐量与延迟的权衡

3. **使用方向类型**
   - `chan<- T`：只发送（生产者）
   - `<-chan T`：只接收（消费者）

4. **总是提供取消路径**
   - 每个发送/接收都放在 select 中
   - 包含 `ctx.Done()` case

### 常见错误

1. **Goroutine 泄漏**：channel 阻塞导致 goroutine 永远等待
2. **向已关闭的 channel 发送**：导致 panic
3. **从错误的 goroutine 关闭**：多发送者场景下可能 panic
4. **无缓冲用于异步通知**：应该用 buffer=1
5. **忘记关闭导致 range 死锁**：range 会等待 close

