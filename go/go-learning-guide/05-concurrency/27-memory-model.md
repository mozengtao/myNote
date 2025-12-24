# Topic 27: Go Memory Model (High-Level Understanding)

## 1. What You Need to Know

The Go memory model defines when reads of a variable in one goroutine are guaranteed to observe writes in another.

**Key guarantee**: Channel operations and mutex locks create "happens-before" relationships.

## 2. Happens-Before Rules

```
┌─────────────────────────────────────────────────────────────────┐
│                    Happens-Before Relationships                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Channel send happens-before receive completes                │
│     ch <- x                                                      │
│        │                                                         │
│        ▼ happens-before                                         │
│     y = <-ch                                                    │
│                                                                  │
│  2. close() happens-before receive returns ok=false              │
│                                                                  │
│  3. mutex.Lock() happens-before subsequent Lock()                │
│                                                                  │
│  4. sync.Once.Do() happens-before any Do() returns              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Safe Patterns

```go
// Channel synchronization
done := make(chan struct{})
go func() {
    sharedData = "initialized"
    close(done)  // Signal completion
}()
<-done  // Wait for goroutine
// sharedData is safely "initialized" here

// Mutex synchronization
mu.Lock()
sharedMap["key"] = value
mu.Unlock()

mu.Lock()
v := sharedMap["key"]  // Guaranteed to see value
mu.Unlock()
```

## 4. Unsafe Patterns

```go
// NO GUARANTEE!
var ready bool
var data string

go func() {
    data = "hello"
    ready = true  // Might be reordered!
}()

for !ready {}    // Might never see ready=true
fmt.Println(data) // Might see empty string
```

---

**Summary**: Use channels, mutexes, or atomic operations to synchronize. Don't rely on variable visibility without explicit synchronization. When in doubt, use the race detector.

