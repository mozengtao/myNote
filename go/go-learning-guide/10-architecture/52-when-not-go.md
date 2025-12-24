# Topic 52: When NOT to Use Go

## 1. Go Is Not Ideal For

### GUI Applications
- No native GUI toolkit
- Bindings exist but aren't great
- Better: Swift (macOS), C#/.NET (Windows), Electron

### Machine Learning
- No tensor libraries like NumPy/PyTorch
- No Jupyter ecosystem
- Better: Python with native extensions

### Data Science / Notebooks
- No interactive REPL
- No notebook support
- Better: Python, R, Julia

### Mobile Development
- Possible with gomobile, but limited
- Better: Swift, Kotlin, React Native, Flutter

### Embedded / Real-Time Systems
- GC makes real-time guarantees hard
- No direct hardware access
- Better: C, Rust, embedded C++

### Kernel / Driver Development
- Needs C ABI compatibility
- Needs precise memory control
- Better: C, Rust

### Scripting / Quick Scripts
- Compilation step is friction
- Verbose for one-liners
- Better: Python, Bash, Ruby

## 2. Signs Go Might Be Wrong Choice

| Symptom | Better Alternative |
|---------|-------------------|
| Fighting the GC | Rust, C++ |
| Need generics heavily | (Go 1.18 added them) |
| Complex math/matrix ops | Python + NumPy |
| Want OOP hierarchies | Java, C# |
| Need GUI | Language with native GUI |
| Sub-microsecond latency | C, Rust |

## 3. Go's Sweet Spot

✅ **Use Go for:**
- HTTP APIs and microservices
- CLI tools
- DevOps/infrastructure tools
- Network servers
- Concurrent data processing
- Container tooling (Docker, K8s)

## 4. Decision Framework

```
Is it a server/backend service?
    Yes → Go is probably good
    
Does it need <1ms latency guarantees?
    Yes → Consider C/C++/Rust
    No → Go is fine
    
Is it CPU-intensive math?
    Yes → Python + native libs
    No → Go is fine
    
Does it need a GUI?
    Yes → Use native GUI language
    No → Go is fine
    
Is it a quick script?
    Yes → Python/Bash is faster to write
    No → Go works
```

## 5. Hybrid Approaches

### Go + C
```go
// #cgo LDFLAGS: -lfast_math
// #include "fast_math.h"
import "C"

func Calculate(x float64) float64 {
    return float64(C.fast_calculate(C.double(x)))
}
```

### Go Calling Python
```go
// Use gRPC or HTTP to call Python service
resp, _ := http.Post("http://ml-service/predict", ...)
```

---

**Summary**: Go excels at networked services and tools. For GUIs, ML, embedded systems, or extreme performance, choose specialized tools. Mix languages when appropriate.

