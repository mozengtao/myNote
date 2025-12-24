# When to Optimize: Profile First

## 1. Engineering Problem

**Premature optimization wastes time and hurts readability.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    OPTIMIZATION PROCESS                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. Write clear, correct code                                          │
│   2. Measure performance                                                │
│   3. Profile to find bottlenecks                                        │
│   4. Optimize only hot paths                                            │
│   5. Measure again                                                      │
│                                                                         │
│   "Premature optimization is the root of all evil"                      │
│                                        - Donald Knuth                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. What to Measure

```bash
# Benchmark
go test -bench=. -benchmem

# CPU profile
go test -cpuprofile=cpu.prof -bench=.

# Memory profile
go test -memprofile=mem.prof -bench=.
```

## 3. Optimization Checklist

1. Is it actually slow?
2. Is this a hot path?
3. What does profiling show?
4. Is the fix worth the complexity?
5. Did performance actually improve?

---

## Chinese Explanation (中文解释)

### 优化流程

1. 写清晰正确的代码
2. 测量性能
3. 分析找到瓶颈
4. 只优化热路径
5. 再次测量

### 过早优化的问题

- 浪费时间
- 损害可读性
- 可能没有实际效果

