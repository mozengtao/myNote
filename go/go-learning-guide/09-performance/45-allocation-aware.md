# Topic 45: Allocation-Aware Programming

## 1. Why Allocations Matter

- Each allocation = work for GC
- More allocations = more GC time = higher latency
- Hot paths should minimize allocations

## 2. Finding Allocations

```bash
# Benchmark with allocation stats
go test -bench=. -benchmem

# Escape analysis
go build -gcflags='-m' main.go
```

## 3. Common Allocation Sources

```go
// Slices
s := make([]byte, 1024)  // Allocates

// Maps
m := make(map[string]int)  // Allocates

// Strings
s := "hello" + "world"  // May allocate

// Interface boxing
var i interface{} = 42  // Allocates

// Closures capturing variables
func() { use(x) }  // May allocate x on heap
```

## 4. Reducing Allocations

### Pre-allocate Slices
```go
// Bad
var items []Item
for _, v := range data {
    items = append(items, process(v))
}

// Good
items := make([]Item, 0, len(data))
for _, v := range data {
    items = append(items, process(v))
}
```

### Use sync.Pool
```go
var bufPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 4096)
    },
}

func process() {
    buf := bufPool.Get().([]byte)
    defer bufPool.Put(buf)
    // Use buf...
}
```

### Avoid String Concatenation
```go
// Bad
s := ""
for _, part := range parts {
    s += part  // Allocates each time
}

// Good
var b strings.Builder
for _, part := range parts {
    b.WriteString(part)
}
s := b.String()
```

### Pass Pointers to Large Structs
```go
// Copies 1KB struct
func process(data BigStruct)

// Passes 8-byte pointer
func process(data *BigStruct)
```

## 5. Benchmark Example

```go
func BenchmarkWithAlloc(b *testing.B) {
    for i := 0; i < b.N; i++ {
        _ = make([]byte, 1024)
    }
}
// Result: 1024 B/op, 1 allocs/op

func BenchmarkWithPool(b *testing.B) {
    for i := 0; i < b.N; i++ {
        buf := bufPool.Get().([]byte)
        bufPool.Put(buf)
    }
}
// Result: 0 B/op, 0 allocs/op
```

---

**Summary**: Use `-benchmem` to find allocations. Pre-allocate slices, use sync.Pool for hot paths, avoid string concatenation in loops.

