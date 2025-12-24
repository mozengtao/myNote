# Interface-Driven Design: Accept Interfaces

## 1. Engineering Problem

**"Accept interfaces, return structs" enables flexible, testable code.**

```go
// GOOD: Accept interface
func ProcessRoutes(store RouteStore) error {
    // Works with any implementation
}

// GOOD: Return concrete type
func NewMemoryStore() *MemoryStore {
    return &MemoryStore{...}
}

// Define interfaces where used (consumer side)
// Keep interfaces small (1-3 methods)
```

## 2. Interface Design Rules

```go
// GOOD: Small, focused interface
type Reader interface {
    Read(p []byte) (n int, err error)
}

// BAD: Too large
type RouteManager interface {
    Add(Route) error
    Get(string) (Route, error)
    Delete(string) error
    List() []Route
    Update(Route) error
    // ... 20 more methods
}
```

## 3. Consumer-Defined Interfaces

```go
// In service package - defines what it needs
type RouteStore interface {
    Get(key string) (Route, error)
}

// In storage package - implements more
type MemoryStore struct { ... }
func (m *MemoryStore) Get(key string) (Route, error) { ... }
func (m *MemoryStore) Add(r Route) error { ... }
func (m *MemoryStore) Delete(key string) error { ... }
```

---

## Chinese Explanation (中文解释)

### 接口设计原则

1. **接受接口，返回结构体**
2. **接口由消费者定义**
3. **保持接口小**（1-3 方法）

### 优势

- 灵活性
- 可测试性
- 解耦

