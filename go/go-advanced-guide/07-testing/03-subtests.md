# Subtests: t.Run

## 1. Engineering Problem

**Subtests provide isolation, filtering, and parallel execution.**

```go
func TestRouteManager(t *testing.T) {
    rm := NewRouteManager()
    
    t.Run("Add", func(t *testing.T) {
        err := rm.Add(Route{Prefix: "10.0.0.0/24"})
        if err != nil {
            t.Fatal(err)
        }
    })
    
    t.Run("Get", func(t *testing.T) {
        r, ok := rm.Get("10.0.0.0/24")
        if !ok {
            t.Fatal("not found")
        }
        if r.Prefix != "10.0.0.0/24" {
            t.Errorf("wrong prefix")
        }
    })
}
```

## 2. Running Specific Subtests

```bash
go test -run TestRouteManager/Add
go test -run TestRouteManager/Get
```

## 3. Parallel Subtests

```go
func TestParallel(t *testing.T) {
    tests := []string{"a", "b", "c"}
    
    for _, tc := range tests {
        tc := tc  // Capture!
        t.Run(tc, func(t *testing.T) {
            t.Parallel()
            // Test runs in parallel
        })
    }
}
```

---

## Chinese Explanation (中文解释)

### t.Run 优势

- 隔离测试用例
- 支持 -run 过滤
- 支持并行执行
- 更好的错误报告

