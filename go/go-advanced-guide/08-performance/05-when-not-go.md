# When Not to Use Go

## 1. Engineering Problem

**Go is excellent for many use cases but not all.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GO STRENGTHS AND LIMITATIONS                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   GO IS GREAT FOR:                                                      │
│   • Network services, APIs                                              │
│   • Microservices                                                       │
│   • CLI tools                                                           │
│   • Infrastructure (Docker, Kubernetes)                                 │
│   • Concurrent systems                                                  │
│                                                                         │
│   CONSIDER ALTERNATIVES FOR:                                            │
│   • Hard real-time systems → C/C++, Rust                               │
│   • UI/Desktop apps → native toolkits                                   │
│   • Data science/ML → Python                                            │
│   • Low-level system programming → C, Rust                             │
│   • Maximum performance-critical → C++, Rust                           │
│                                                                         │
│   GO LIMITATIONS:                                                       │
│   • GC pauses (usually <1ms, but not deterministic)                    │
│   • No manual memory management                                         │
│   • Limited generics (improving)                                        │
│   • No inheritance (by design)                                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Decision Guide

| Use Case | Best Tool |
|----------|-----------|
| Web API | **Go**, Node.js |
| CLI tool | **Go**, Rust |
| Systems programming | Rust, C |
| Data science | Python |
| Game engine | C++, Rust |

---

## Chinese Explanation (中文解释)

### Go 适合

- 网络服务、API
- 微服务
- 命令行工具
- 基础设施

### 考虑其他语言

- 硬实时系统 → C/Rust
- GUI 应用 → 原生工具
- 数据科学 → Python
- 极致性能 → C++/Rust

