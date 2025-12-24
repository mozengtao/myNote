# go test: Test Functions and Conventions

## 1. Engineering Problem

**Go has built-in testing with no external dependencies.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GO TESTING                                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   File naming:     xxx_test.go                                          │
│   Function naming: TestXxx(t *testing.T)                                │
│   Run:             go test ./...                                        │
│                                                                         │
│   t.Error/t.Errorf - Report failure, continue                           │
│   t.Fatal/t.Fatalf - Report failure, stop test                          │
│   t.Log/t.Logf     - Log message (shown with -v)                        │
│   t.Skip/t.Skipf   - Skip test                                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Basic Test

```go
// route_test.go
package route

import "testing"

func TestNewRoute(t *testing.T) {
    r := NewRoute("10.0.0.0/24")
    if r.Prefix != "10.0.0.0/24" {
        t.Errorf("got %q, want %q", r.Prefix, "10.0.0.0/24")
    }
}
```

## 3. Running Tests

```bash
go test              # Current package
go test ./...        # All packages
go test -v           # Verbose
go test -run TestXxx # Specific test
go test -race        # Race detector
go test -cover       # Coverage
```

## 4. Test Helpers

```go
func setupRoutes(t *testing.T) *RouteManager {
    t.Helper()  // Marks as helper
    rm := NewRouteManager()
    if err := rm.Load("testdata/routes.json"); err != nil {
        t.Fatalf("setup: %v", err)
    }
    return rm
}
```

---

## Chinese Explanation (中文解释)

### 测试命名

- 文件：`xxx_test.go`
- 函数：`TestXxx(t *testing.T)`

### 常用方法

| 方法 | 作用 |
|------|------|
| t.Error | 报告失败，继续 |
| t.Fatal | 报告失败，停止 |
| t.Helper | 标记辅助函数 |

