# Concurrency Patterns: Worker Pools, Pipelines, Fan-In/Out

## 1. Engineering Problem

### What real-world problem does this solve?

**Concurrency patterns provide battle-tested solutions for parallel processing.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CONCURRENCY PATTERNS                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. WORKER POOL: Bounded parallelism                                   │
│   ─────────────────────────────────                                     │
│                                                                         │
│   jobs ──► [Worker 1] ──┐                                               │
│        ──► [Worker 2] ──┼──► results                                    │
│        ──► [Worker 3] ──┘                                               │
│                                                                         │
│   Use when: Process many tasks with limited goroutines                  │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   2. PIPELINE: Staged processing                                        │
│   ─────────────────────────────                                         │
│                                                                         │
│   input ──► [Stage 1] ──► [Stage 2] ──► [Stage 3] ──► output           │
│             (parse)       (validate)     (store)                        │
│                                                                         │
│   Use when: Multi-step transformation                                   │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   3. FAN-OUT/FAN-IN: Parallel then aggregate                            │
│   ──────────────────────────────────────────                            │
│                                                                         │
│                 ┌──► [Worker 1] ──┐                                     │
│   input ───────┼──► [Worker 2] ──┼───► merge ──► output                │
│    (fan-out)   └──► [Worker 3] ──┘    (fan-in)                         │
│                                                                         │
│   Use when: Split work, then combine results                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Worker Pool

```go
func workerPool(workers int, jobs <-chan Job, results chan<- Result) {
    var wg sync.WaitGroup
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for job := range jobs {
                results <- process(job)
            }
        }()
    }
    wg.Wait()
    close(results)
}
```

### Pipeline

```go
func pipeline() <-chan Result {
    // Stage 1: Generate
    nums := generate()
    
    // Stage 2: Square
    squared := square(nums)
    
    // Stage 3: Filter
    filtered := filter(squared)
    
    return filtered
}
```

### Fan-Out/Fan-In

```go
// Fan-out: Start multiple workers on same input
func fanOut(in <-chan Job, workers int) []<-chan Result {
    outs := make([]<-chan Result, workers)
    for i := 0; i < workers; i++ {
        outs[i] = worker(in)
    }
    return outs
}

// Fan-in: Merge multiple channels into one
func fanIn(channels ...<-chan Result) <-chan Result {
    out := make(chan Result)
    var wg sync.WaitGroup
    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan Result) {
            defer wg.Done()
            for v := range c {
                out <- v
            }
        }(ch)
    }
    go func() {
        wg.Wait()
        close(out)
    }()
    return out
}
```

---

## 3. Language Mechanism

### Worker pool with context

```go
func WorkerPool(ctx context.Context, workers int, jobs <-chan Job) <-chan Result {
    results := make(chan Result, workers)
    
    var wg sync.WaitGroup
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for {
                select {
                case job, ok := <-jobs:
                    if !ok {
                        return
                    }
                    result := process(job)
                    select {
                    case results <- result:
                    case <-ctx.Done():
                        return
                    }
                case <-ctx.Done():
                    return
                }
            }
        }(i)
    }
    
    go func() {
        wg.Wait()
        close(results)
    }()
    
    return results
}
```

### Pipeline with cancellation

```go
func pipeline(ctx context.Context, input <-chan Route) <-chan Route {
    // Stage 1: Validate
    validated := make(chan Route)
    go func() {
        defer close(validated)
        for r := range input {
            select {
            case <-ctx.Done():
                return
            default:
                if r.IsValid() {
                    validated <- r
                }
            }
        }
    }()
    
    // Stage 2: Enrich
    enriched := make(chan Route)
    go func() {
        defer close(enriched)
        for r := range validated {
            select {
            case <-ctx.Done():
                return
            default:
                r.Enriched = true
                enriched <- r
            }
        }
    }()
    
    return enriched
}
```

### Semaphore pattern

```go
// Limit concurrent operations
type Semaphore chan struct{}

func NewSemaphore(n int) Semaphore {
    return make(chan struct{}, n)
}

func (s Semaphore) Acquire() { s <- struct{}{} }
func (s Semaphore) Release() { <-s }

func processWithLimit(items []Item, limit int) {
    sem := NewSemaphore(limit)
    var wg sync.WaitGroup
    
    for _, item := range items {
        wg.Add(1)
        sem.Acquire()
        go func(item Item) {
            defer wg.Done()
            defer sem.Release()
            process(item)
        }(item)
    }
    
    wg.Wait()
}
```

---

## 4. Idiomatic Usage

### Error handling in worker pool

```go
type Result struct {
    Route Route
    Err   error
}

func processRoutes(ctx context.Context, routes []Route, workers int) ([]Route, error) {
    jobs := make(chan Route, len(routes))
    results := make(chan Result, len(routes))
    
    // Start workers
    var wg sync.WaitGroup
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for route := range jobs {
                result := processRoute(ctx, route)
                results <- result
            }
        }()
    }
    
    // Send jobs
    for _, r := range routes {
        jobs <- r
    }
    close(jobs)
    
    // Collect results
    go func() {
        wg.Wait()
        close(results)
    }()
    
    var processed []Route
    var firstErr error
    for r := range results {
        if r.Err != nil && firstErr == nil {
            firstErr = r.Err
        }
        if r.Err == nil {
            processed = append(processed, r.Route)
        }
    }
    
    return processed, firstErr
}
```

### Rate limiting

```go
func rateLimitedWorker(ctx context.Context, jobs <-chan Job, rps int) <-chan Result {
    results := make(chan Result)
    ticker := time.NewTicker(time.Second / time.Duration(rps))
    
    go func() {
        defer close(results)
        defer ticker.Stop()
        
        for {
            select {
            case <-ctx.Done():
                return
            case <-ticker.C:
                select {
                case job, ok := <-jobs:
                    if !ok {
                        return
                    }
                    results <- process(job)
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    
    return results
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Unbounded goroutines

```go
// BAD: Creates goroutine per item
for _, item := range items {
    go process(item)  // Could create millions of goroutines!
}

// GOOD: Use worker pool
pool := NewWorkerPool(10)
for _, item := range items {
    pool.Submit(item)
}
```

### Pitfall 2: Forgetting to close channels

```go
// BAD: results channel never closed
func process(jobs <-chan Job) <-chan Result {
    results := make(chan Result)
    go func() {
        for job := range jobs {
            results <- processJob(job)
        }
        // Forgot close(results)!
    }()
    return results
}

// GOOD: Always close output channel
func process(jobs <-chan Job) <-chan Result {
    results := make(chan Result)
    go func() {
        defer close(results)  // Always close
        for job := range jobs {
            results <- processJob(job)
        }
    }()
    return results
}
```

### Pitfall 3: Not handling ctx.Done

```go
// BAD: Ignores cancellation
for job := range jobs {
    result := slowProcess(job)
    results <- result
}

// GOOD: Check context
for job := range jobs {
    select {
    case <-ctx.Done():
        return
    default:
    }
    result := slowProcess(job)
    select {
    case results <- result:
    case <-ctx.Done():
        return
    }
}
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

type RouteResult struct {
    Route Route
    Err   error
}

// Worker pool with proper lifecycle management
type RouteProcessor struct {
    workers int
    jobs    chan Route
    results chan RouteResult
    wg      sync.WaitGroup
}

func NewRouteProcessor(workers, bufferSize int) *RouteProcessor {
    return &RouteProcessor{
        workers: workers,
        jobs:    make(chan Route, bufferSize),
        results: make(chan RouteResult, bufferSize),
    }
}

func (p *RouteProcessor) Start(ctx context.Context) {
    for i := 0; i < p.workers; i++ {
        p.wg.Add(1)
        go p.worker(ctx, i)
    }
    
    // Close results when all workers done
    go func() {
        p.wg.Wait()
        close(p.results)
    }()
}

func (p *RouteProcessor) worker(ctx context.Context, id int) {
    defer p.wg.Done()
    
    for {
        select {
        case <-ctx.Done():
            fmt.Printf("Worker %d: shutting down\n", id)
            return
        case route, ok := <-p.jobs:
            if !ok {
                fmt.Printf("Worker %d: jobs channel closed\n", id)
                return
            }
            
            result := p.processRoute(ctx, route)
            
            select {
            case p.results <- result:
            case <-ctx.Done():
                return
            }
        }
    }
}

func (p *RouteProcessor) processRoute(ctx context.Context, r Route) RouteResult {
    // Simulate processing
    select {
    case <-time.After(100 * time.Millisecond):
        fmt.Printf("Processed: %s\n", r.Prefix)
        return RouteResult{Route: r}
    case <-ctx.Done():
        return RouteResult{Err: ctx.Err()}
    }
}

func (p *RouteProcessor) Submit(r Route) {
    p.jobs <- r
}

func (p *RouteProcessor) Close() {
    close(p.jobs)
}

func (p *RouteProcessor) Results() <-chan RouteResult {
    return p.results
}

// Pipeline pattern
func routePipeline(ctx context.Context, routes []Route) <-chan Route {
    // Stage 1: Generator
    gen := func() <-chan Route {
        out := make(chan Route)
        go func() {
            defer close(out)
            for _, r := range routes {
                select {
                case out <- r:
                case <-ctx.Done():
                    return
                }
            }
        }()
        return out
    }
    
    // Stage 2: Validator
    validate := func(in <-chan Route) <-chan Route {
        out := make(chan Route)
        go func() {
            defer close(out)
            for r := range in {
                if r.Prefix != "" {
                    select {
                    case out <- r:
                    case <-ctx.Done():
                        return
                    }
                }
            }
        }()
        return out
    }
    
    // Stage 3: Enricher
    enrich := func(in <-chan Route) <-chan Route {
        out := make(chan Route)
        go func() {
            defer close(out)
            for r := range in {
                r.NextHop = "enriched:" + r.NextHop
                select {
                case out <- r:
                case <-ctx.Done():
                    return
                }
            }
        }()
        return out
    }
    
    return enrich(validate(gen()))
}

// Fan-out/Fan-in
func fanOutFanIn(ctx context.Context, routes []Route, workers int) []Route {
    // Fan-out: Multiple workers read from same channel
    jobs := make(chan Route)
    go func() {
        defer close(jobs)
        for _, r := range routes {
            select {
            case jobs <- r:
            case <-ctx.Done():
                return
            }
        }
    }()
    
    // Start workers
    results := make([]<-chan Route, workers)
    for i := 0; i < workers; i++ {
        out := make(chan Route)
        go func(in <-chan Route, out chan<- Route) {
            defer close(out)
            for r := range in {
                // Process
                r.NextHop = "processed"
                select {
                case out <- r:
                case <-ctx.Done():
                    return
                }
            }
        }(jobs, out)
        results[i] = out
    }
    
    // Fan-in: Merge all results
    merged := make(chan Route)
    var wg sync.WaitGroup
    for _, ch := range results {
        wg.Add(1)
        go func(c <-chan Route) {
            defer wg.Done()
            for r := range c {
                select {
                case merged <- r:
                case <-ctx.Done():
                    return
                }
            }
        }(ch)
    }
    go func() {
        wg.Wait()
        close(merged)
    }()
    
    // Collect
    var output []Route
    for r := range merged {
        output = append(output, r)
    }
    return output
}

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    
    // Worker Pool Example
    fmt.Println("=== Worker Pool ===")
    processor := NewRouteProcessor(3, 10)
    processor.Start(ctx)
    
    routes := []Route{
        {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        {VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"},
        {VrfID: 2, Prefix: "172.16.0.0/16", NextHop: "10.0.0.1"},
    }
    
    for _, r := range routes {
        processor.Submit(r)
    }
    processor.Close()
    
    for result := range processor.Results() {
        if result.Err != nil {
            fmt.Printf("Error: %v\n", result.Err)
        } else {
            fmt.Printf("Result: %+v\n", result.Route)
        }
    }
    
    // Pipeline Example
    fmt.Println("\n=== Pipeline ===")
    for r := range routePipeline(ctx, routes) {
        fmt.Printf("Pipeline output: %+v\n", r)
    }
    
    // Fan-Out/Fan-In Example
    fmt.Println("\n=== Fan-Out/Fan-In ===")
    output := fanOutFanIn(ctx, routes, 2)
    for _, r := range output {
        fmt.Printf("Fan-in output: %+v\n", r)
    }
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CONCURRENCY PATTERN RULES                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   WORKER POOL:                                                          │
│   • Use when: Bounded parallelism needed                                │
│   • Size: Start with GOMAXPROCS, tune based on workload                 │
│   • Always: Close jobs channel, wait for workers                        │
│                                                                         │
│   PIPELINE:                                                             │
│   • Use when: Multi-stage transformation                                │
│   • Each stage: Own goroutine, reads from prev, writes to next          │
│   • Always: Close output channel when input exhausted                   │
│                                                                         │
│   FAN-OUT/FAN-IN:                                                       │
│   • Fan-out: Multiple workers on same input channel                     │
│   • Fan-in: Merge multiple channels into one                            │
│   • Use WaitGroup for synchronization                                   │
│                                                                         │
│   GENERAL RULES:                                                        │
│   • Always check ctx.Done() in loops                                    │
│   • Close channels when done writing                                    │
│   • Use buffered channels to prevent blocking                           │
│   • Handle errors within workers                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 工作池（Worker Pool）

- **用途**：有限并行度
- **大小**：从 GOMAXPROCS 开始，根据负载调优
- **要点**：关闭 jobs 通道，等待 workers

### 管道（Pipeline）

- **用途**：多阶段转换
- **每阶段**：独立 goroutine，从前一阶段读，写入下一阶段
- **要点**：输入耗尽时关闭输出通道

### 扇出/扇入（Fan-Out/Fan-In）

- **扇出**：多个 worker 读同一输入通道
- **扇入**：合并多个通道为一个
- **同步**：使用 WaitGroup

### 通用规则

1. 在循环中检查 ctx.Done()
2. 写完后关闭通道
3. 使用缓冲通道防止阻塞
4. 在 worker 中处理错误

