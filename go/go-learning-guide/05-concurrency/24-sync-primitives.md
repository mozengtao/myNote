# Topic 24: Synchronization Primitives (mutex, RWMutex, atomic)

## 1. Problem It Solves

When channels aren't the right fit:
- Protecting shared data structures (maps, counters)
- Fine-grained locking for performance
- Simple atomic operations

## 2. sync.Mutex

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

## 3. sync.RWMutex

```go
var (
    mu    sync.RWMutex
    cache = make(map[string]string)
)

func get(key string) string {
    mu.RLock()  // Multiple readers OK
    defer mu.RUnlock()
    return cache[key]
}

func set(key, value string) {
    mu.Lock()  // Exclusive write
    defer mu.Unlock()
    cache[key] = value
}
```

## 4. sync/atomic

```go
var counter int64

func increment() {
    atomic.AddInt64(&counter, 1)
}

func get() int64 {
    return atomic.LoadInt64(&counter)
}
```

## 5. From routermgr_grpc.go

```go
var (
    addressMutex sync.Mutex
    routeMutex   sync.Mutex
)

func (s *routermgrServer) AddRouteV4(...) {
    routeMutex.Lock()
    // Modify VmcRoutes map
    routeMutex.Unlock()
}
```

## 6. When to Use What

| Pattern | Use |
|---------|-----|
| Channel | Communication, pipelines |
| Mutex | Protecting shared state |
| RWMutex | Read-heavy workloads |
| atomic | Simple counters/flags |

---

**Summary**: Use `sync.Mutex` for exclusive access, `sync.RWMutex` for read-heavy workloads, and `sync/atomic` for simple counters. Prefer channels for communication between goroutines.

