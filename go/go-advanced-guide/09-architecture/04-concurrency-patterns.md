# Concurrency Patterns: Worker Pools and Pipelines

## 1. Worker Pool

```go
func workerPool(workers int, jobs <-chan Job) <-chan Result {
    results := make(chan Result, workers)
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
    
    go func() {
        wg.Wait()
        close(results)
    }()
    
    return results
}
```

## 2. Pipeline

```go
func pipeline(input <-chan int) <-chan int {
    // Stage 1
    doubled := make(chan int)
    go func() {
        defer close(doubled)
        for n := range input {
            doubled <- n * 2
        }
    }()
    
    // Stage 2
    filtered := make(chan int)
    go func() {
        defer close(filtered)
        for n := range doubled {
            if n > 10 {
                filtered <- n
            }
        }
    }()
    
    return filtered
}
```

## 3. Fan-Out/Fan-In

```go
// Fan-out: multiple workers read from same channel
// Fan-in: merge multiple channels into one

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

## Chinese Explanation (中文解释)

### 模式

| 模式 | 用途 |
|------|------|
| Worker Pool | 有限并行度 |
| Pipeline | 阶段处理 |
| Fan-Out/In | 并行后聚合 |

