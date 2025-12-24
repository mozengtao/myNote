# Topic 25: Context Package (Cancellation, Deadlines)

## 1. Problem It Solves

- How to cancel long-running operations?
- How to set deadlines/timeouts?
- How to pass request-scoped values?

## 2. Core API

```go
import "context"

// Create contexts
ctx := context.Background()              // Root context
ctx := context.TODO()                    // Placeholder

ctx, cancel := context.WithCancel(parent)      // Cancellable
ctx, cancel := context.WithTimeout(parent, 5*time.Second)  // Timeout
ctx, cancel := context.WithDeadline(parent, deadline)      // Deadline
ctx := context.WithValue(parent, key, value)   // Values

// Check cancellation
select {
case <-ctx.Done():
    return ctx.Err()  // Cancelled or DeadlineExceeded
default:
    // Continue work
}
```

## 3. Usage Pattern

```go
func (s *routermgrServer) AddRouteV4(ctx context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // Check if request already cancelled
    if ctx.Err() != nil {
        return nil, ctx.Err()
    }
    
    // Pass context to downstream operations
    if err := callExternalService(ctx); err != nil {
        return nil, err
    }
    
    return &routermgrpb.RouteActionResponse{Success: true}, nil
}

func callExternalService(ctx context.Context) error {
    select {
    case <-ctx.Done():
        return ctx.Err()
    case result := <-doWork():
        return handleResult(result)
    }
}
```

## 4. Rules

1. First parameter to functions: `func DoSomething(ctx context.Context, ...)`
2. Don't store in structs (pass explicitly)
3. Cancel contexts when done: `defer cancel()`
4. Don't pass nil context (use `context.TODO()`)

---

**Summary**: Context provides cancellation, deadlines, and request-scoped values. Pass as first parameter, always defer cancel(), and check `ctx.Done()` in long operations.

