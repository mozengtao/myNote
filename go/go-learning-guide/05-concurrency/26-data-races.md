# Topic 26: Data Races and How to Avoid Them

## 1. What is a Data Race?

A data race occurs when:
1. Two goroutines access the same variable
2. At least one is a write
3. No synchronization

```go
// DATA RACE!
var count int

go func() { count++ }()
go func() { count++ }()
```

## 2. Detection

```bash
go run -race main.go
go test -race ./...
```

## 3. How to Fix

### Option 1: Mutex
```go
var (
    mu    sync.Mutex
    count int
)

func increment() {
    mu.Lock()
    defer mu.Unlock()
    count++
}
```

### Option 2: Channels
```go
func counter(ch chan int) {
    count := 0
    for delta := range ch {
        count += delta
    }
}
```

### Option 3: Atomic
```go
var count int64
atomic.AddInt64(&count, 1)
```

## 4. From routermgr_grpc.go

```go
var (
    addressMutex sync.Mutex  // Protects RouterAddresses, VmcAddresses
    routeMutex   sync.Mutex  // Protects VmcRoutes
)

// Map access protected
routeMutex.Lock()
VmcRoutes[route.VmcName][VmcRoute{...}] = false
routeMutex.Unlock()
```

## 5. Common Causes

- Forgetting to lock before map access
- Race on loop variable in goroutine
- Unprotected struct field access

---

**Summary**: Use `-race` flag to detect data races. Fix with mutex, channels, or atomics. Always protect shared mutable state.

