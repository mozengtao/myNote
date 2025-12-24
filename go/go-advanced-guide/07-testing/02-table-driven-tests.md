# Table-Driven Tests: Systematic Testing Pattern

## 1. Engineering Problem

### What real-world problem does this solve?

**Table-driven tests eliminate test code duplication while making it trivial to add new test cases.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TEST DUPLICATION PROBLEM                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Without table-driven:              With table-driven:                 │
│   ─────────────────────              ───────────────────                │
│                                                                         │
│   func TestParseValid(t *testing.T)  func TestParse(t *testing.T) {     │
│   func TestParseEmpty(t *testing.T)      tests := []struct{...}{        │
│   func TestParseInvalid(t *testing.T)        {name: "valid", ...},      │
│   func TestParseIPv6(t *testing.T)           {name: "empty", ...},      │
│   // 20 more functions...                    {name: "invalid", ...},    │
│                                              // Easy to add more!       │
│   Problems:                              }                              │
│   • Repeated boilerplate                 for _, tt := range tests {     │
│   • Easy to forget edge cases                t.Run(tt.name, ...)        │
│   • Hard to see all cases at once        }                              │
│   • Inconsistent error messages      }                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Tests become unmaintainable with many similar functions
- Edge cases get missed because adding tests is tedious
- Inconsistent assertion patterns across tests
- Hard to understand complete test coverage at a glance

---

## 2. Core Mental Model

### How Go expects you to think

**Separate TEST DATA from TEST LOGIC. Data is a table, logic runs once.**

```go
// Test data - what varies between cases
tests := []struct {
    name    string      // Descriptive name for t.Run
    input   string      // Input to function
    want    Result      // Expected output
    wantErr bool        // Whether error expected
}{
    {"valid input", "10.0.0.0/24", Route{...}, false},
    {"empty input", "", Route{}, true},
}

// Test logic - same for all cases
for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        got, err := Parse(tt.input)
        // assertions...
    })
}
```

### Philosophy

- Each row is a complete test case
- Names describe the scenario, not implementation
- Easy to add cases: just add a row
- All cases visible in one place

---

## 3. Language Mechanism

### Basic structure

```go
func TestFunction(t *testing.T) {
    tests := []struct {
        name     string
        input    InputType
        want     OutputType
        wantErr  bool
    }{
        // Test cases...
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := Function(tt.input)
            
            if (err != nil) != tt.wantErr {
                t.Errorf("error = %v, wantErr %v", err, tt.wantErr)
                return
            }
            
            if !reflect.DeepEqual(got, tt.want) {
                t.Errorf("got %v, want %v", got, tt.want)
            }
        })
    }
}
```

### t.Run creates subtests

```go
// Running specific subtest
// go test -run TestParse/valid_IPv4

t.Run("valid IPv4", func(t *testing.T) {
    // This is a subtest
})
```

### Parallel subtests

```go
for _, tt := range tests {
    tt := tt  // CRITICAL: Capture range variable
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()  // Run in parallel
        // test logic
    })
}
```

---

## 4. Idiomatic Usage

### When to use

- Testing pure functions with multiple inputs
- Validation logic with many edge cases
- Parsing/formatting functions
- Any function where you can enumerate inputs/outputs

### When NOT to use

- Tests requiring complex setup per case
- Tests with side effects that can't be isolated
- Integration tests with external dependencies
- When table would have only 1-2 rows

### Pattern: Setup function in struct

```go
tests := []struct {
    name    string
    setup   func() *RouteManager
    input   Route
    wantErr bool
}{
    {
        name: "empty store",
        setup: func() *RouteManager {
            return NewRouteManager()
        },
        input: Route{Prefix: "10.0.0.0/24"},
    },
    {
        name: "with existing route",
        setup: func() *RouteManager {
            rm := NewRouteManager()
            rm.Add(Route{Prefix: "10.0.0.0/24"})
            return rm
        },
        input: Route{Prefix: "10.0.0.0/24"},
        wantErr: true,  // Duplicate
    },
}

for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        rm := tt.setup()
        err := rm.Add(tt.input)
        // assertions
    })
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Forgetting to capture range variable

```go
// BAD: All goroutines see same tt!
for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        check(tt.input)  // Race condition: tt changes!
    })
}

// GOOD: Capture tt
for _, tt := range tests {
    tt := tt  // Shadow and capture
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        check(tt.input)  // Each goroutine has its own tt
    })
}
```

### Pitfall 2: Unclear test names

```go
// BAD: Names don't describe scenarios
tests := []struct {
    name string
    // ...
}{
    {"test1", ...},
    {"test2", ...},
}

// GOOD: Names describe what's being tested
tests := []struct {
    name string
    // ...
}{
    {"valid IPv4 prefix", ...},
    {"empty prefix returns error", ...},
    {"IPv6 prefix with zone ID", ...},
}
```

### Pitfall 3: Not using t.Run

```go
// BAD: No subtests, hard to identify failures
for _, tt := range tests {
    got := Parse(tt.input)
    if got != tt.want {
        t.Errorf("Parse(%q) = %v, want %v", tt.input, got, tt.want)
    }
}

// GOOD: Subtests identify which case failed
for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        got := Parse(tt.input)
        if got != tt.want {
            t.Errorf("got %v, want %v", got, tt.want)
        }
    })
}
```

### Pitfall 4: Too much logic in table

```go
// BAD: Complex setup obscures test intent
tests := []struct {
    setup func() (*DB, *Cache, *Logger, *Config, error)
    // ...
}

// GOOD: Keep table data simple, move complexity to helpers
tests := []struct {
    name     string
    scenario Scenario  // Enum: EmptyStore, WithRoutes, etc.
}

func setupScenario(s Scenario) *RouteManager { ... }
```

---

## 6. Complete, Realistic Example

```go
package route

import (
    "errors"
    "net"
    "testing"
)

type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
    IsV6    bool
}

var ErrInvalidPrefix = errors.New("invalid prefix")
var ErrInvalidNextHop = errors.New("invalid next hop")

func ParseRoute(s string) (Route, error) {
    _, _, err := net.ParseCIDR(s)
    if err != nil {
        return Route{}, ErrInvalidPrefix
    }
    // Simplified parsing
    return Route{Prefix: s}, nil
}

func ValidateRoute(r Route) error {
    if r.Prefix == "" {
        return ErrInvalidPrefix
    }
    if r.NextHop == "" {
        return ErrInvalidNextHop
    }
    return nil
}

// Table-driven test for ParseRoute
func TestParseRoute(t *testing.T) {
    tests := []struct {
        name    string
        input   string
        want    Route
        wantErr error
    }{
        {
            name:  "valid IPv4 /24",
            input: "10.0.0.0/24",
            want:  Route{Prefix: "10.0.0.0/24"},
        },
        {
            name:  "valid IPv4 /32",
            input: "192.168.1.1/32",
            want:  Route{Prefix: "192.168.1.1/32"},
        },
        {
            name:  "valid IPv6",
            input: "2001:db8::/32",
            want:  Route{Prefix: "2001:db8::/32"},
        },
        {
            name:    "empty string",
            input:   "",
            wantErr: ErrInvalidPrefix,
        },
        {
            name:    "no prefix length",
            input:   "10.0.0.0",
            wantErr: ErrInvalidPrefix,
        },
        {
            name:    "invalid IP",
            input:   "999.999.999.999/24",
            wantErr: ErrInvalidPrefix,
        },
        {
            name:    "negative prefix",
            input:   "10.0.0.0/-1",
            wantErr: ErrInvalidPrefix,
        },
        {
            name:    "prefix too large",
            input:   "10.0.0.0/33",
            wantErr: ErrInvalidPrefix,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := ParseRoute(tt.input)
            
            // Check error
            if tt.wantErr != nil {
                if !errors.Is(err, tt.wantErr) {
                    t.Errorf("error = %v, wantErr %v", err, tt.wantErr)
                }
                return
            }
            if err != nil {
                t.Fatalf("unexpected error: %v", err)
            }
            
            // Check result
            if got != tt.want {
                t.Errorf("got %+v, want %+v", got, tt.want)
            }
        })
    }
}

// Table-driven test for ValidateRoute
func TestValidateRoute(t *testing.T) {
    tests := []struct {
        name    string
        route   Route
        wantErr error
    }{
        {
            name: "valid route",
            route: Route{
                VrfID:   1,
                Prefix:  "10.0.0.0/24",
                NextHop: "192.168.1.1",
            },
            wantErr: nil,
        },
        {
            name: "missing prefix",
            route: Route{
                VrfID:   1,
                NextHop: "192.168.1.1",
            },
            wantErr: ErrInvalidPrefix,
        },
        {
            name: "missing next hop",
            route: Route{
                VrfID:  1,
                Prefix: "10.0.0.0/24",
            },
            wantErr: ErrInvalidNextHop,
        },
        {
            name:    "empty route",
            route:   Route{},
            wantErr: ErrInvalidPrefix,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidateRoute(tt.route)
            
            if tt.wantErr != nil {
                if !errors.Is(err, tt.wantErr) {
                    t.Errorf("error = %v, wantErr %v", err, tt.wantErr)
                }
                return
            }
            
            if err != nil {
                t.Errorf("unexpected error: %v", err)
            }
        })
    }
}

// Parallel table-driven test
func TestParseRouteParallel(t *testing.T) {
    tests := []struct {
        name  string
        input string
        valid bool
    }{
        {"ipv4-a", "10.0.0.0/24", true},
        {"ipv4-b", "172.16.0.0/16", true},
        {"ipv6-a", "2001:db8::/32", true},
        {"invalid", "not-a-route", false},
    }

    for _, tt := range tests {
        tt := tt  // Capture for parallel execution
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel()  // Run in parallel
            
            _, err := ParseRoute(tt.input)
            gotValid := err == nil
            
            if gotValid != tt.valid {
                t.Errorf("ParseRoute(%q) valid=%v, want valid=%v",
                    tt.input, gotValid, tt.valid)
            }
        })
    }
}
```

Run tests:
```bash
go test -v -run TestParseRoute
go test -v -run TestParseRoute/valid_IPv4
go test -v -run TestParseRouteParallel
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TABLE-DRIVEN TEST RULES                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ALWAYS USE t.Run                                                   │
│      • Creates subtests                                                 │
│      • Enables -run filtering                                           │
│      • Clear failure identification                                     │
│                                                                         │
│   2. CAPTURE RANGE VARIABLE FOR PARALLEL                                │
│      • tt := tt before t.Run                                            │
│      • Required when using t.Parallel()                                 │
│                                                                         │
│   3. NAME TESTS BY SCENARIO                                             │
│      • "valid IPv4 prefix" not "test1"                                  │
│      • Names appear in test output                                      │
│                                                                         │
│   4. INCLUDE BOTH POSITIVE AND NEGATIVE CASES                           │
│      • Valid inputs                                                     │
│      • Edge cases                                                       │
│      • Error conditions                                                 │
│                                                                         │
│   5. KEEP TABLE DATA SIMPLE                                             │
│      • Inputs and expected outputs                                      │
│      • Move complex setup to helper functions                           │
│                                                                         │
│   6. USE errors.Is FOR ERROR COMPARISON                                 │
│      • Not == for error comparison                                      │
│      • Works with wrapped errors                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 表驱动测试核心结构

```go
tests := []struct {
    name    string      // 测试名称
    input   InputType   // 输入
    want    OutputType  // 期望输出
    wantErr bool        // 是否期望错误
}{
    {"case1", input1, want1, false},
    {"case2", input2, want2, true},
}

for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        // 测试逻辑
    })
}
```

### 常见陷阱

1. **忘记捕获循环变量**：`tt := tt` 必须在并行测试中
2. **测试名称不清晰**：应描述场景，非 "test1"
3. **不使用 t.Run**：无法单独运行子测试
4. **表中逻辑太复杂**：保持数据简单

### 最佳实践

- 总是用 `t.Run` 创建子测试
- 并行测试前捕获循环变量
- 用场景命名测试
- 包含正向和负向测试
- 用 `errors.Is` 比较错误
