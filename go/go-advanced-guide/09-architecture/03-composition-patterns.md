# Composition Patterns: No Inheritance

## 1. Engineering Problem

**Go uses composition and embedding instead of inheritance.**

```go
// Embedding for code reuse
type BaseRoute struct {
    Prefix  string
    NextHop string
}

type VrfRoute struct {
    BaseRoute  // Embedded
    VrfID uint32
}

// Methods are promoted
route := VrfRoute{
    BaseRoute: BaseRoute{Prefix: "10.0.0.0/24"},
    VrfID:     1,
}
fmt.Println(route.Prefix)  // Promoted from BaseRoute
```

## 2. Composition vs Embedding

```go
// Embedding: IS-A relationship (promoted methods)
type Logger struct {
    *log.Logger  // Methods promoted
}

// Composition: HAS-A relationship (explicit access)
type Service struct {
    logger *log.Logger  // Access via service.logger
}
```

## 3. Interface Composition

```go
type Reader interface { Read([]byte) (int, error) }
type Writer interface { Write([]byte) (int, error) }

// Composed interface
type ReadWriter interface {
    Reader
    Writer
}
```

---

## Chinese Explanation (中文解释)

### Go 的组合

- 嵌入：方法提升，IS-A
- 组合：显式访问，HAS-A

### 接口组合

```go
type ReadWriter interface {
    Reader
    Writer
}
```

