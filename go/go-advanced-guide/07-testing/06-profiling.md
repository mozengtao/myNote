# Profiling with pprof

## 1. Engineering Problem

**pprof identifies performance bottlenecks in production code.**

## 2. CPU Profiling

```go
// In tests
go test -cpuprofile=cpu.prof -bench=.
go tool pprof cpu.prof

// In application
import _ "net/http/pprof"
go func() {
    http.ListenAndServe(":6060", nil)
}()

// Access
// http://localhost:6060/debug/pprof/
```

## 3. Memory Profiling

```bash
go test -memprofile=mem.prof -bench=.
go tool pprof mem.prof

# In pprof
(pprof) top10
(pprof) list FunctionName
(pprof) web  # Visual graph
```

## 4. Common Commands

```bash
# Interactive
go tool pprof cpu.prof
(pprof) top         # Top functions
(pprof) top -cum    # By cumulative time
(pprof) list Func   # Source view
(pprof) web         # Graph in browser

# One-liner
go tool pprof -top cpu.prof
```

---

## Chinese Explanation (中文解释)

### 性能分析类型

| 类型 | 用途 |
|------|------|
| CPU | 找出耗时函数 |
| Memory | 找出分配热点 |
| Goroutine | 找出 goroutine 问题 |

### 关键命令

- `top` - 顶级函数
- `list` - 源码视图
- `web` - 可视化图

