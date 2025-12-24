# Topic 37: Time, Context, and Cancellation Patterns

## 1. Time Package

```go
import "time"

// Current time
now := time.Now()
utc := now.UTC()

// Durations
d := 5 * time.Second
d := time.Hour + 30*time.Minute

// Parsing/Formatting
t, _ := time.Parse("2006-01-02", "2024-01-15")  // Reference time!
s := t.Format("Jan 2, 2006")

// Comparisons
if t1.Before(t2) { }
if t1.After(t2) { }
duration := t2.Sub(t1)
```

## 2. Timers and Tickers

```go
// One-shot timer
timer := time.NewTimer(5 * time.Second)
<-timer.C  // Blocks for 5 seconds

// Or simpler
time.Sleep(5 * time.Second)

// Ticker (repeated)
ticker := time.NewTicker(1 * time.Second)
defer ticker.Stop()

for {
    select {
    case <-ticker.C:
        fmt.Println("tick")
    case <-done:
        return
    }
}
```

## 3. Context Patterns

```go
import "context"

// Timeout
ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
defer cancel()

select {
case result := <-doWork(ctx):
    return result
case <-ctx.Done():
    return ctx.Err()  // DeadlineExceeded or Canceled
}

// Deadline (specific time)
deadline := time.Now().Add(5 * time.Second)
ctx, cancel := context.WithDeadline(context.Background(), deadline)
defer cancel()

// Value (request-scoped data)
ctx := context.WithValue(parentCtx, "requestID", "abc123")
reqID := ctx.Value("requestID").(string)
```

## 4. Combining Patterns

```go
func fetchWithRetry(ctx context.Context, url string) ([]byte, error) {
    for attempt := 1; attempt <= 3; attempt++ {
        select {
        case <-ctx.Done():
            return nil, ctx.Err()
        default:
        }
        
        data, err := fetch(ctx, url)
        if err == nil {
            return data, nil
        }
        
        // Exponential backoff
        backoff := time.Duration(attempt) * time.Second
        select {
        case <-time.After(backoff):
        case <-ctx.Done():
            return nil, ctx.Err()
        }
    }
    return nil, errors.New("max retries exceeded")
}
```

## 5. From routermgr_grpc.go

```go
// Context is first parameter in gRPC handlers
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // Context can be used for:
    // - Cancellation checking
    // - Deadline enforcement
    // - Request-scoped values (trace ID, auth info)
}
```

---

**Summary**: time package for durations/parsing, timers/tickers for scheduling, context for cancellation/deadlines/values. Always pass context as first parameter and honor ctx.Done().

