# Topic 41: Profiling with pprof

## 1. Types of Profiles

- **CPU**: Where is time spent?
- **Memory (Heap)**: What allocates memory?
- **Goroutine**: How many goroutines? What are they doing?
- **Block**: Where do goroutines block?
- **Mutex**: Where is lock contention?

## 2. HTTP Profiler (Recommended)

```go
import _ "net/http/pprof"

func main() {
    go func() {
        log.Println(http.ListenAndServe("localhost:6060", nil))
    }()
    // Your application...
}
```

Access at:
- http://localhost:6060/debug/pprof/
- http://localhost:6060/debug/pprof/heap
- http://localhost:6060/debug/pprof/goroutine

## 3. Command Line Analysis

```bash
# CPU profile
go tool pprof http://localhost:6060/debug/pprof/profile?seconds=30

# Heap profile
go tool pprof http://localhost:6060/debug/pprof/heap

# In pprof:
(pprof) top
(pprof) top --cum
(pprof) list FunctionName
(pprof) web  # Opens in browser
```

## 4. Benchmark Profiling

```bash
# Generate profile
go test -bench=. -cpuprofile=cpu.prof
go test -bench=. -memprofile=mem.prof

# Analyze
go tool pprof cpu.prof
```

## 5. Programmatic Profiling

```go
import "runtime/pprof"

f, _ := os.Create("cpu.prof")
pprof.StartCPUProfile(f)
defer pprof.StopCPUProfile()

// Code to profile...
```

## 6. Common Commands in pprof

```
top         - Top functions by time
top --cum   - Top by cumulative time
list foo    - Source of function foo
web         - Open SVG in browser
png > out.png - Save as PNG
```

---

**Summary**: Import `net/http/pprof` for easy profiling. Use `go tool pprof` to analyze. Focus on CPU for performance, heap for memory issues.

