# Topic 44: When Go Is Slow

## 1. Go Struggles With

### CPU-Intensive Computations
```go
// Pure number crunching
for i := 0; i < 1000000000; i++ {
    result += math.Sin(float64(i))
}
// C/C++/Rust would be faster
```

### Low-Level Memory Control
- No SIMD intrinsics (without assembly)
- No manual memory layout control
- GC pauses (though minimal in modern Go)

### Systems Programming Edge Cases
- Device drivers
- Real-time systems (hard deadlines)
- Kernel modules

## 2. Specific Slowdowns

### Interface Overhead
```go
// Virtual call overhead
type Interface interface { Method() }

func process(i Interface) {
    i.Method()  // Indirect call
}

// vs direct call
func processDirect(c Concrete) {
    c.Method()  // Faster
}
```

### Reflection
```go
// Slow
reflect.ValueOf(obj).Field(0).Interface()

// Fast
obj.Field
```

### Excessive Allocation
```go
// Bad: allocates every call
func process() {
    data := make([]byte, 4096)  // Heap allocation
}

// Better: reuse
var pool = sync.Pool{...}
```

## 3. Compared to C/C++

| Scenario | Go Speed vs C |
|----------|---------------|
| Matrix multiplication | 50-70% |
| Sorting | 70-90% |
| JSON parsing | 80-100% |
| HTTP serving | 90-100% |
| Image processing | 60-80% |

## 4. When to Consider Alternatives

- Pure compute (C/C++, Rust)
- ML/numerics (Python with C extensions)
- Systems kernel (C, Rust)
- Game engines (C++)

---

**Summary**: Go is slower for CPU-bound computation, low-level memory control, and reflection-heavy code. For these, consider C/C++/Rust or optimized libraries.

