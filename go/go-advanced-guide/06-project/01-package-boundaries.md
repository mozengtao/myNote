# Package Boundaries and Visibility

## 1. Engineering Problem

**Packages define visibility boundaries and organize code by responsibility.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PACKAGE VISIBILITY                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Exported (Uppercase):            Unexported (lowercase):              │
│   ────────────────────             ────────────────────────             │
│   type Route struct { }            type routeStore struct { }           │
│   func NewRoute() *Route           func validate(r *Route) error        │
│   const MaxRoutes = 1000           var defaultTTL = time.Hour           │
│                                                                         │
│   • Visible outside package        • Package-private                    │
│   • Part of API contract           • Implementation detail              │
│   • Must maintain compatibility    • Can change freely                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Package Naming

```go
// GOOD: Short, lowercase, no underscores
package route
package config
package auth

// BAD
package routeManager  // camelCase
package route_manager // underscores
package routes        // plural (usually)
```

## 3. internal/ Directory

```
myproject/
├── cmd/server/main.go
├── internal/           ← Go enforces: can't import from outside
│   └── route/
│       └── route.go
└── pkg/               ← Importable by external code
    └── api/
        └── types.go
```

## 4. What to Export

```go
package route

// Export types callers need
type Route struct {
    VrfID   uint32
    Prefix  string
    NextHop string
}

// Export constructors
func NewRoute(prefix string) *Route { ... }

// Hide implementation
type routeValidator struct { ... }  // unexported
```

---

## Chinese Explanation (中文解释)

### 可见性规则

| 首字母 | 可见性 |
|--------|--------|
| 大写 | 导出（公开） |
| 小写 | 未导出（私有） |

### internal/ 目录

- Go 工具链强制执行
- 只能被父目录导入
- 用于内部实现

