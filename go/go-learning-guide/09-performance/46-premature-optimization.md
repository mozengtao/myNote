# Topic 46: Avoiding Premature Optimization

## 1. The Rule

> "Premature optimization is the root of all evil" - Donald Knuth

**Profile first, optimize second.**

## 2. Wrong Approach

```go
// Over-engineering before measuring
type OptimizedBuffer struct {
    data   [64]byte  // Stack-allocated for speed!
    pooled bool      // Track if from pool
    // ...complex logic...
}

// Just use bytes.Buffer until profiling shows it's slow
var buf bytes.Buffer
```

## 3. Right Approach

### Step 1: Write Clear Code
```go
func processData(data []byte) []byte {
    result := transform(data)
    result = filter(result)
    return result
}
```

### Step 2: Benchmark
```bash
go test -bench=BenchmarkProcessData -benchmem
```

### Step 3: Profile
```bash
go test -bench=. -cpuprofile=cpu.prof
go tool pprof cpu.prof
```

### Step 4: Optimize Hotspots Only
```go
// ONLY if profiling shows transform() is slow:
func transform(data []byte) []byte {
    // Optimized implementation
}
```

## 4. Optimization Checklist

Before optimizing, ask:
- [ ] Is this actually slow? (benchmarks)
- [ ] Is this a hotspot? (profiles)
- [ ] Will optimization complicate the code?
- [ ] What's the expected improvement?

## 5. Common Unnecessary Optimizations

```go
// Unnecessary: compiler handles this
x = x << 1  // Instead of x * 2

// Unnecessary: GC is fast
object = nil  // "Help" GC

// Unnecessary: unless benchmarks show otherwise
sync.Pool for everything

// Unnecessary: compiler inlines small functions
inline keyword (doesn't exist in Go anyway)
```

## 6. When to Optimize

- After profiling shows a hotspot
- When latency SLOs are missed
- When memory usage is problematic
- When benchmarks show regression

---

**Summary**: Write clear code first. Benchmark. Profile. Only optimize what profiling shows is slow. Most code doesn't need optimization.

