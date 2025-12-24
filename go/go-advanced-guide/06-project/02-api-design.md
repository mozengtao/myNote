# API Design: Stable Interfaces

## 1. Engineering Problem

**Good API design balances usability, stability, and extensibility.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    API DESIGN PRINCIPLES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. Accept interfaces, return structs                                  │
│   2. Make zero values useful                                            │
│   3. Keep interfaces small (1-3 methods)                                │
│   4. Return errors, don't panic                                         │
│   5. Use options for configuration                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Accept Interfaces, Return Structs

```go
// GOOD: Accept interface
func ProcessRoutes(r io.Reader) error { ... }

// GOOD: Return concrete type
func NewRouteManager() *RouteManager { ... }
```

## 3. Functional Options Pattern

```go
type RouteManager struct {
    timeout time.Duration
    maxSize int
}

type Option func(*RouteManager)

func WithTimeout(d time.Duration) Option {
    return func(rm *RouteManager) {
        rm.timeout = d
    }
}

func NewRouteManager(opts ...Option) *RouteManager {
    rm := &RouteManager{
        timeout: 30 * time.Second,  // default
        maxSize: 1000,              // default
    }
    for _, opt := range opts {
        opt(rm)
    }
    return rm
}

// Usage
rm := NewRouteManager(
    WithTimeout(time.Minute),
    WithMaxSize(5000),
)
```

## 4. Useful Zero Values

```go
// GOOD: Zero value is usable
type Cache struct {
    mu    sync.Mutex  // Zero value works
    items map[string]Item
}

func (c *Cache) Get(key string) (Item, bool) {
    c.mu.Lock()
    defer c.mu.Unlock()
    if c.items == nil {
        return Item{}, false
    }
    item, ok := c.items[key]
    return item, ok
}
```

---

## Chinese Explanation (中文解释)

### API 设计原则

1. **接受接口，返回结构体**
2. **使零值可用**
3. **保持接口小**
4. **返回错误，不要 panic**
5. **用选项模式配置**

