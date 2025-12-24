# Topic 39: Benchmarks and Performance Testing

## 1. Benchmark Basics

```go
// mycode_test.go
func BenchmarkFunction(b *testing.B) {
    for i := 0; i < b.N; i++ {
        Function()
    }
}
```

## 2. Running Benchmarks

```bash
go test -bench=.                    # All benchmarks
go test -bench=BenchmarkFunction    # Specific
go test -bench=. -benchmem          # Include memory stats
go test -bench=. -count=5           # Run 5 times
go test -bench=. -benchtime=5s      # Run for 5 seconds
```

## 3. Output

```
BenchmarkFunction-8    1000000    1234 ns/op    256 B/op    3 allocs/op
                  │         │           │            │           │
                  │         │           │            │           └── allocations per op
                  │         │           │            └── bytes per op
                  │         │           └── time per operation
                  │         └── number of operations
                  └── GOMAXPROCS
```

## 4. Sub-benchmarks

```go
func BenchmarkEncode(b *testing.B) {
    sizes := []int{1, 10, 100, 1000}
    
    for _, size := range sizes {
        b.Run(fmt.Sprintf("size=%d", size), func(b *testing.B) {
            data := make([]byte, size)
            b.ResetTimer()
            for i := 0; i < b.N; i++ {
                encode(data)
            }
        })
    }
}
```

## 5. Avoiding Compiler Optimization

```go
var result int  // Package-level to prevent optimization

func BenchmarkCalculate(b *testing.B) {
    var r int
    for i := 0; i < b.N; i++ {
        r = Calculate(i)
    }
    result = r  // Store to prevent dead code elimination
}
```

## 6. Memory Benchmarks

```go
func BenchmarkAlloc(b *testing.B) {
    b.ReportAllocs()  // Report allocations
    
    for i := 0; i < b.N; i++ {
        _ = make([]byte, 1024)
    }
}
```

---

**Summary**: Use `testing.B` for benchmarks, `b.N` for iterations, `-benchmem` for allocations. Use sub-benchmarks for comparing different inputs.

