# Topic 38: go test and Table-Driven Tests

## 1. Test File Convention

```go
// mypackage/calc.go
package mypackage

func Add(a, b int) int {
    return a + b
}

// mypackage/calc_test.go  (same package)
package mypackage

import "testing"

func TestAdd(t *testing.T) {
    result := Add(2, 3)
    if result != 5 {
        t.Errorf("Add(2, 3) = %d; want 5", result)
    }
}
```

## 2. Run Tests

```bash
go test                    # Current package
go test ./...              # All packages
go test -v                 # Verbose
go test -run TestAdd       # Specific test
go test -cover             # With coverage
go test -race              # With race detector
```

## 3. Table-Driven Tests (Idiomatic Go)

```go
func TestAdd(t *testing.T) {
    tests := []struct {
        name     string
        a, b     int
        expected int
    }{
        {"positive", 2, 3, 5},
        {"negative", -1, -1, -2},
        {"zero", 0, 0, 0},
        {"mixed", -1, 5, 4},
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            result := Add(tt.a, tt.b)
            if result != tt.expected {
                t.Errorf("Add(%d, %d) = %d; want %d",
                    tt.a, tt.b, result, tt.expected)
            }
        })
    }
}
```

## 4. Test Helpers

```go
func TestSomething(t *testing.T) {
    // Subtests
    t.Run("subtest", func(t *testing.T) { })
    
    // Skip
    t.Skip("not implemented")
    
    // Parallel
    t.Parallel()
    
    // Helper
    t.Helper()  // Marks function as test helper
    
    // Cleanup
    t.Cleanup(func() { /* runs after test */ })
}
```

---

**Summary**: Test files end in `_test.go`. Use table-driven tests. Run with `go test -v ./...`.

