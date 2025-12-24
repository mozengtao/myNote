# Topic 47: Comparing Go Performance to C/C++

## 1. Typical Performance Ratio

| Workload | Go vs C/C++ |
|----------|-------------|
| Web servers | 90-100% |
| JSON handling | 80-95% |
| Database drivers | 85-95% |
| Sorting algorithms | 70-90% |
| Matrix operations | 50-70% |
| Tight numeric loops | 40-60% |

## 2. Why Go Is Slower

### GC Overhead
```
C: No GC, manual control
Go: ~1-5% CPU for GC, sub-ms pauses
```

### Bounds Checking
```go
arr[i] = value  // Go checks bounds
```
```c
arr[i] = value;  // C trusts you
```

### Interface Dispatch
```go
var r io.Reader = file
r.Read(buf)  // Indirect call via interface
```
```c
read(fd, buf, len);  // Direct syscall
```

## 3. Why Go Is Comparable (or Better)

### Goroutines vs Threads
```
Go: 1M goroutines, 2KB each
C: 10K threads, 1MB each (practical limit)
```

### Development Speed
```
Go: Edit → Build (1s) → Test
C++: Edit → Build (10m) → Debug segfault
```

### Safety = Fewer Bugs
```go
// Go: Panic with stack trace
arr := []int{1, 2, 3}
_ = arr[10]  // panic: index out of range

// C: Undefined behavior
int arr[3] = {1, 2, 3};
arr[10];  // Who knows?
```

## 4. When to Use C/C++ Instead

- Device drivers
- Operating system kernels
- Real-time systems (hard deadlines)
- HPC / scientific computing
- Game engine core loops
- When every nanosecond matters

## 5. When Go Is the Better Choice

- Web services (productivity wins)
- Microservices
- CLI tools
- DevOps tooling
- Network proxies
- Distributed systems

## 6. Real Example: Docker

Docker is written in Go:
- Handles thousands of containers
- Network-heavy I/O
- Needs to be reliable
- Development speed matters

Could be faster in C? Maybe 10-20%. Worth the development cost? No.

---

**Summary**: Go is 70-100% of C/C++ speed for most server workloads. The productivity gain usually outweighs the small performance difference. Use C/C++ only when you truly need maximum performance.

