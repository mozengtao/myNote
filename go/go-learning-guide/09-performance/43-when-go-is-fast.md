# Topic 43: When Go Is Fast

## 1. Go Excels At

### I/O-Bound Workloads
```go
// Handling 10k+ concurrent connections
func handleConnection(conn net.Conn) {
    // Each in its own goroutine
    // Runtime efficiently multiplexes onto OS threads
}
```

### Network Services
- HTTP servers (net/http is highly optimized)
- gRPC services
- Proxies and load balancers
- Microservices

### Concurrent Processing
```go
// Fan-out, fan-in pattern
func processItems(items []Item) {
    results := make(chan Result, len(items))
    for _, item := range items {
        go func(i Item) {
            results <- process(i)
        }(item)
    }
    // Collect results...
}
```

## 2. Why Go Is Fast Here

1. **Goroutine scheduling**: 1M goroutines, minimal overhead
2. **Non-blocking I/O**: Runtime handles epoll/kqueue
3. **Fast compilation**: Quick iteration cycles
4. **Simple memory model**: Fewer cache misses
5. **Static binary**: No runtime overhead

## 3. Benchmarks (Approximate)

| Scenario | Go vs Java | Go vs Python |
|----------|------------|--------------|
| HTTP handler | Similar | 10-50x faster |
| JSON parsing | Similar | 20-50x faster |
| Concurrent I/O | Better | 100x+ better |
| String processing | Slower | 5-10x faster |

## 4. From routermgr_grpc.go

```go
// gRPC server handles concurrent requests efficiently
grpcServer := grpc.NewServer()
grpcServer.Serve(lis)

// Each RPC is a goroutine - cheap to spawn
// Mutex protects shared state with minimal overhead
routeMutex.Lock()
VmcRoutes[route.VmcName][...] = false
routeMutex.Unlock()
```

---

**Summary**: Go excels at I/O-bound, concurrent workloads. Network servers, microservices, and tools that need high concurrency are Go's sweet spot.

