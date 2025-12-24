# Topic 23: Select and Concurrency Orchestration

## 1. Problem It Solves (Engineering Motivation)

How to wait on multiple channel operations?
- Multiple data sources
- Timeouts
- Cancellation signals
- Non-blocking operations

```
┌─────────────────────────────────────────────────────────────────┐
│                    Multi-Channel Problem                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Without select:               With select:                      │
│                                                                  │
│  // Can only wait on one       select {                         │
│  data := <-ch1                 case data := <-ch1:              │
│  // ch2 blocked!                   // handle ch1                │
│                                case data := <-ch2:              │
│                                    // handle ch2                │
│                                case <-timeout:                  │
│                                    // handle timeout            │
│                                }                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
`select` 语句让你同时等待多个 channel 操作。哪个 channel 先就绪就执行哪个分支。这是实现超时、取消、多路复用等并发模式的关键。

## 2. Core Idea and Mental Model

**select is like a switch for channel operations. It blocks until one case can proceed.**

```go
select {
case v := <-ch1:
    // ch1 received
case ch2 <- x:
    // ch2 sent
case <-time.After(time.Second):
    // timeout
default:
    // non-blocking: runs if no case ready
}
```

## 3. Common Patterns

### Timeout Pattern
```go
select {
case result := <-work:
    return result, nil
case <-time.After(5 * time.Second):
    return nil, errors.New("timeout")
}
```

### Cancellation Pattern
```go
select {
case <-ctx.Done():
    return ctx.Err()
case data := <-ch:
    process(data)
}
```

### Non-blocking Receive
```go
select {
case msg := <-ch:
    fmt.Println("Received:", msg)
default:
    fmt.Println("No message available")
}
```

### Fan-in Pattern
```go
func merge(ch1, ch2 <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for ch1 != nil || ch2 != nil {
            select {
            case v, ok := <-ch1:
                if !ok { ch1 = nil; continue }
                out <- v
            case v, ok := <-ch2:
                if !ok { ch2 = nil; continue }
                out <- v
            }
        }
    }()
    return out
}
```

## 4. Example

```go
func worker(ctx context.Context, jobs <-chan Job) {
    for {
        select {
        case <-ctx.Done():
            fmt.Println("Worker cancelled")
            return
        case job := <-jobs:
            process(job)
        }
    }
}
```

---

**Summary**: `select` multiplexes channel operations, enabling timeouts, cancellation, and complex concurrency patterns. It's essential for writing robust concurrent Go code.

