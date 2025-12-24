# Versioning: Semantic Versioning in Go

## 1. Engineering Problem

### What real-world problem does this solve?

**Version numbers communicate compatibility promises between library authors and users.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SEMANTIC VERSIONING                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   v1.2.3                                                                │
│   │ │ │                                                                 │
│   │ │ └── Patch: Bug fixes (backward compatible)                        │
│   │ └──── Minor: New features (backward compatible)                     │
│   └────── Major: Breaking changes (incompatible)                        │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Go's special rule for v2+:                                            │
│                                                                         │
│   v0.x.x: Unstable - anything can change                               │
│   v1.x.x: Stable - semver rules apply                                  │
│   v2.x.x: DIFFERENT MODULE PATH! (go.mod: module pkg/v2)               │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Breaking changes include:                                             │
│   • Removing exported functions/types                                   │
│   • Changing function signatures                                        │
│   • Changing struct field types                                         │
│   • Changing interface methods                                          │
│   • Changing behavior in incompatible ways                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Breaking changes in minor versions break downstream builds
- v2+ without /v2 path causes import confusion
- Pre-1.0 libraries suddenly making breaking changes without warning
- Diamond dependency problems with incompatible versions

---

## 2. Core Mental Model

### How Go expects you to think

**Each major version is a DIFFERENT MODULE. v1 and v2 can coexist.**

```go
// go.mod for v1
module github.com/company/router

// go.mod for v2 - different path!
module github.com/company/router/v2
```

### Import paths

```go
// Importing v1
import "github.com/company/router"

// Importing v2 - different path
import routerv2 "github.com/company/router/v2"

// Both can coexist in same project!
```

### Philosophy

- Major version = breaking change = new module
- Users explicitly opt into breaking changes via import path
- Old code continues to work with old version

---

## 3. Language Mechanism

### Version tags

```bash
# Create version tag
git tag v1.0.0
git push origin v1.0.0

# v2+ needs /v2 in go.mod AND tag
# go.mod: module github.com/company/router/v2
git tag v2.0.0
git push origin v2.0.0
```

### go.mod require

```go
module myapp

go 1.21

require (
    github.com/company/router v1.5.0      // v1
    github.com/company/router/v2 v2.1.0   // v2 (different module!)
)
```

### Version selection

```bash
# Get specific version
go get github.com/company/router@v1.5.0

# Get latest v1
go get github.com/company/router@v1

# Get latest (including pre-release)
go get github.com/company/router@latest

# Get pseudo-version from commit
go get github.com/company/router@a1b2c3d
```

### Pre-release versions

```bash
# Alpha/beta/rc versions
v1.0.0-alpha.1
v1.0.0-beta.2
v1.0.0-rc.1
v1.0.0

# Not selected by default
go get pkg@latest  # Gets v1.0.0, not v1.0.1-alpha.1
go get pkg@v1.0.1-alpha.1  # Explicit required
```

---

## 4. Idiomatic Usage

### When to bump major version

- Removing or renaming exported identifiers
- Changing function/method signatures
- Changing struct fields (add OK, remove/rename not OK)
- Adding required interface methods
- Changing fundamental behavior

### When to bump minor version

- Adding new exported functions/types
- Adding new optional struct fields
- Adding new interface implementations
- Feature additions

### When to bump patch version

- Bug fixes
- Performance improvements
- Documentation updates
- Internal refactoring

### v2+ directory structure options

```
# Option 1: /v2 subdirectory (recommended)
router/
├── go.mod           # module github.com/company/router
├── router.go        # v1 code
└── v2/
    ├── go.mod       # module github.com/company/router/v2
    └── router.go    # v2 code

# Option 2: Major version branch
main branch: v1
v2 branch: contains go.mod with /v2
```

---

## 5. Common Pitfalls

### Pitfall 1: v2 without /v2 in module path

```go
// BAD: go.mod says v2 but no /v2
// go.mod
module github.com/company/router  // Wrong for v2!

// git tag v2.0.0  // Won't work!

// GOOD: v2 must have /v2 in path
// go.mod
module github.com/company/router/v2  // Correct!
```

### Pitfall 2: Breaking changes in minor version

```go
// v1.0.0
func ParseRoute(s string) Route { ... }

// v1.1.0 - BAD: Breaking change!
func ParseRoute(s string) (Route, error) { ... }

// GOOD: Add new function
func ParseRouteE(s string) (Route, error) { ... }
// Or deprecate and add new
// Deprecated: Use ParseRouteV2 instead.
func ParseRoute(s string) Route { ... }
func ParseRouteV2(s string) (Route, error) { ... }
```

### Pitfall 3: Not tagging releases

```go
// Without tags, users get pseudo-versions
require github.com/company/router v0.0.0-20240101120000-abcdef123456

// With proper tags
require github.com/company/router v1.2.3
```

### Pitfall 4: v0.x instability assumptions

```go
// v0.x has no stability guarantees
// But users still expect some stability!

// GOOD: Communicate clearly
// README.md:
// This project is pre-1.0. API may change between minor versions.
// For stability, pin to specific version in go.mod.
```

---

## 6. Complete, Realistic Example

```go
// ========================================
// github.com/company/router/v2 - go.mod
// ========================================
// module github.com/company/router/v2
// 
// go 1.21

// ========================================
// github.com/company/router/v2/router.go
// ========================================
package router

// Route represents a network route.
// v2 BREAKING CHANGE: VrfID changed from int to uint32
type Route struct {
    VrfID   uint32  // Was int in v1
    Prefix  string
    NextHop string
}

// ParseRoute parses a route string.
// v2 BREAKING CHANGE: Now returns error
func ParseRoute(s string) (Route, error) {
    // In v1: func ParseRoute(s string) Route
    // ...
    return Route{}, nil
}

// Migrate converts v1 Route to v2 Route.
// Provided for migration convenience.
func Migrate(v1VrfID int, prefix, nextHop string) Route {
    return Route{
        VrfID:   uint32(v1VrfID),
        Prefix:  prefix,
        NextHop: nextHop,
    }
}

// ========================================
// User's code - using both versions
// ========================================
// go.mod:
// require (
//     github.com/company/router v1.5.0
//     github.com/company/router/v2 v2.0.0
// )

package main

import (
    routerv1 "github.com/company/router"
    routerv2 "github.com/company/router/v2"
)

func main() {
    // Use v1 for legacy code
    oldRoute := routerv1.ParseRoute("10.0.0.0/24")
    
    // Use v2 for new code
    newRoute, err := routerv2.ParseRoute("10.0.0.0/24")
    if err != nil {
        panic(err)
    }
    
    // Migrate v1 to v2
    migrated := routerv2.Migrate(
        oldRoute.VrfID,  // int in v1
        oldRoute.Prefix,
        oldRoute.NextHop,
    )
    
    _ = newRoute
    _ = migrated
}

// ========================================
// Version tags workflow
// ========================================
// 
// # Initial release
// git tag v1.0.0
// git push origin v1.0.0
// 
// # Bug fix
// git tag v1.0.1
// git push origin v1.0.1
// 
// # New feature
// git tag v1.1.0
// git push origin v1.1.0
// 
// # Breaking change - create v2
// # 1. Create v2/ directory with new go.mod
// # 2. Update module path to include /v2
// git tag v2.0.0
// git push origin v2.0.0
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    VERSIONING RULES                                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. FOLLOW SEMVER                                                      │
│      • Major: Breaking changes                                          │
│      • Minor: New features (compatible)                                 │
│      • Patch: Bug fixes                                                 │
│                                                                         │
│   2. v2+ REQUIRES /v2 IN MODULE PATH                                    │
│      • go.mod: module pkg/v2                                            │
│      • Import: import "pkg/v2"                                          │
│      • Tag: git tag v2.0.0                                              │
│                                                                         │
│   3. AVOID BREAKING CHANGES                                             │
│      • Add new functions instead of changing                            │
│      • Deprecate rather than remove                                     │
│      • v2 is painful for users                                          │
│                                                                         │
│   4. TAG RELEASES                                                       │
│      • Users prefer tagged versions                                     │
│      • Pseudo-versions are ugly                                         │
│                                                                         │
│   5. v0.x MEANS UNSTABLE                                                │
│      • Document this clearly                                            │
│      • Consider going 1.0 early                                         │
│                                                                         │
│   6. PROVIDE MIGRATION PATH                                             │
│      • Migration helpers for v1→v2                                      │
│      • Clear changelog                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 语义版本号

| 部分 | 含义 | 兼容性 |
|------|------|--------|
| Major | 破坏性变更 | 不兼容 |
| Minor | 新功能 | 向后兼容 |
| Patch | Bug 修复 | 向后兼容 |

### Go 的特殊规则

- **v0.x.x**：不稳定，任何变更都可能
- **v1.x.x**：稳定，遵循语义版本
- **v2.x.x**：需要不同的模块路径！

### v2+ 规则

```go
// go.mod
module github.com/company/router/v2  // 必须有 /v2

// 导入
import "github.com/company/router/v2"  // 导入路径也要 /v2

// 标签
git tag v2.0.0  // 标签仍是 v2.0.0
```

### 最佳实践

1. 遵循语义版本
2. v2+ 必须有 /v2 路径
3. 避免破坏性变更
4. 总是打标签
5. 提供迁移路径
