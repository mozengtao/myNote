# time Package: Timeouts, Deadlines, and Tickers

## 1. Engineering Problem

**The time package provides timeouts, scheduling, and time operations.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TIME PACKAGE                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   time.Duration:     time.Timer:         time.Ticker:                  │
│   ──────────────     ───────────         ────────────                  │
│   • 5 * time.Second  • One-shot delay    • Repeated events             │
│   • time.Millisecond • Stop/Reset        • Stop when done              │
│                                                                         │
│   time.After:        time.AfterFunc:     time.Sleep:                   │
│   ───────────        ──────────────      ────────────                  │
│   • Select timeout   • Callback timer    • Block goroutine             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Timeouts with select

```go
func fetchWithTimeout(ctx context.Context) (Result, error) {
    select {
    case result := <-doFetch():
        return result, nil
    case <-time.After(5 * time.Second):
        return Result{}, errors.New("timeout")
    case <-ctx.Done():
        return Result{}, ctx.Err()
    }
}
```

## 3. Tickers for periodic work

```go
func cleanup(ctx context.Context) {
    ticker := time.NewTicker(time.Minute)
    defer ticker.Stop()  // Always stop!
    
    for {
        select {
        case <-ticker.C:
            doCleanup()
        case <-ctx.Done():
            return
        }
    }
}
```

## 4. Timers

```go
// One-shot timer
timer := time.NewTimer(5 * time.Second)
defer timer.Stop()

select {
case <-timer.C:
    fmt.Println("Timer fired")
case <-done:
    if !timer.Stop() {
        <-timer.C  // Drain if already fired
    }
}
```

## 5. Common Pitfalls

```go
// BAD: Ticker leak
func bad() {
    for range time.Tick(time.Second) {  // Never stopped!
        // ...
    }
}

// GOOD: Stop ticker
func good(ctx context.Context) {
    ticker := time.NewTicker(time.Second)
    defer ticker.Stop()
    for {
        select {
        case <-ticker.C:
            // ...
        case <-ctx.Done():
            return
        }
    }
}
```

---

## Chinese Explanation (中文解释)

### 主要类型

| 类型 | 用途 |
|------|------|
| `time.Duration` | 时间间隔 |
| `time.Timer` | 一次性定时器 |
| `time.Ticker` | 周期性定时器 |

### 关键规则

1. **总是 Stop() Ticker**：防止泄漏
2. **用 select 实现超时**
3. **用 context 而非硬编码超时**

