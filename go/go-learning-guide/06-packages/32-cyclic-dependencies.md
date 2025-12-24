# Topic 32: Avoiding Cyclic Dependencies

## 1. Problem

Go does not allow import cycles:
```
package a imports package b
package b imports package a  // ERROR!
```

## 2. Detection

```bash
go build ./...
# import cycle not allowed
# package a
#     imports b
#     imports a
```

## 3. Solutions

### Solution 1: Interface in Consumer

```go
// Bad: a imports b, b imports a

// Good: define interface where it's used
package b

type Processor interface {  // Define interface in consumer
    Process(data []byte) error
}

func Run(p Processor) {
    p.Process(data)
}

// package a implements the interface
// package a imports b (one direction only)
```

### Solution 2: Common Package

```go
// Extract shared types to a third package
myapp/
├── types/       # Shared types, no imports
│   └── types.go
├── a/           # imports types
└── b/           # imports types
```

### Solution 3: Dependency Injection

```go
// Instead of importing the other package,
// accept it as a parameter

type Handler struct {
    processor ProcessorInterface  // Injected, not imported
}
```

## 4. Signs of Cycle Problems

- Two packages "need" each other
- God package that does everything
- Circular function calls

## 5. Prevention

```
┌─────────────────────────────────────────────────────────────────┐
│                    Dependency Direction                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  cmd/server          (imports everything, imports nothing)       │
│      │                                                           │
│      ▼                                                           │
│  internal/handler    (imports service)                           │
│      │                                                           │
│      ▼                                                           │
│  internal/service    (imports repository)                        │
│      │                                                           │
│      ▼                                                           │
│  internal/repository (imports nothing internal)                  │
│      │                                                           │
│      ▼                                                           │
│  pkg/types           (shared types, imports nothing)             │
│                                                                  │
│  Dependencies flow DOWN, never UP                                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

**Summary**: Use interfaces at boundaries, extract shared types to common packages, use dependency injection. Dependencies should flow one direction (typically inward).

