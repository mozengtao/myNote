# Error Boundaries: API Design for Errors

## 1. Engineering Problem

### What real-world problem does this solve?

**Error boundaries define where errors should be handled vs propagated.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR BOUNDARY LAYERS                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                         HTTP Handler                            │   │
│   │  • Convert errors to HTTP status codes                          │   │
│   │  • Log errors with request context                              │   │
│   │  • Hide internal details from clients                           │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                               │                                         │
│                               ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                       Service Layer                             │   │
│   │  • Add business context to errors                               │   │
│   │  • Translate domain errors                                      │   │
│   │  • May retry or fallback                                        │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                               │                                         │
│                               ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                      Repository Layer                           │   │
│   │  • Wrap database/storage errors                                 │   │
│   │  • Translate SQL errors to domain errors                        │   │
│   │  • Add entity context (IDs, keys)                              │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                               │                                         │
│                               ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                     Infrastructure                              │   │
│   │  • Database, network, filesystem errors                         │   │
│   │  • Low-level system errors                                      │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Each layer's responsibility

```go
// Repository: Translate infrastructure → domain
func (r *RouteRepo) Get(key string) (Route, error) {
    row := r.db.QueryRow("SELECT * FROM routes WHERE key = ?", key)
    if err := row.Scan(...); err != nil {
        if err == sql.ErrNoRows {
            return Route{}, ErrNotFound  // Domain error
        }
        return Route{}, fmt.Errorf("query route %s: %w", key, err)
    }
    return route, nil
}

// Service: Add business context
func (s *RouteService) GetRoute(vrfID uint32, prefix string) (Route, error) {
    route, err := s.repo.Get(key(vrfID, prefix))
    if err != nil {
        return Route{}, fmt.Errorf("get route VRF %d: %w", vrfID, err)
    }
    return route, nil
}

// Handler: Convert to HTTP response
func (h *Handler) GetRoute(w http.ResponseWriter, r *http.Request) {
    route, err := h.service.GetRoute(vrfID, prefix)
    if errors.Is(err, ErrNotFound) {
        http.Error(w, "Route not found", http.StatusNotFound)
        return
    }
    if err != nil {
        log.Printf("Error: %v", err)  // Log full error
        http.Error(w, "Internal error", http.StatusInternalServerError)
        return
    }
    json.NewEncoder(w).Encode(route)
}
```

---

## 3. Language Mechanism

### Domain errors

```go
package route

// Domain errors - part of API contract
var (
    ErrNotFound     = errors.New("route not found")
    ErrInvalidRoute = errors.New("invalid route")
    ErrVrfNotExist  = errors.New("VRF does not exist")
)

// Error type for errors needing context
type ValidationError struct {
    Field   string
    Message string
}
```

### Error translation

```go
// Translate SQL errors to domain errors
func (r *RouteRepo) Get(key string) (Route, error) {
    err := r.db.QueryRow(...).Scan(...)
    switch {
    case err == sql.ErrNoRows:
        return Route{}, ErrNotFound
    case err != nil:
        return Route{}, fmt.Errorf("db query: %w", err)
    }
    return route, nil
}
```

---

## 4. Idiomatic Usage

### HTTP error mapping

```go
type APIError struct {
    Status  int    `json:"-"`
    Code    string `json:"code"`
    Message string `json:"message"`
}

func mapError(err error) APIError {
    switch {
    case errors.Is(err, ErrNotFound):
        return APIError{
            Status:  http.StatusNotFound,
            Code:    "NOT_FOUND",
            Message: "Resource not found",
        }
    case errors.Is(err, ErrInvalidRoute):
        return APIError{
            Status:  http.StatusBadRequest,
            Code:    "INVALID_INPUT",
            Message: "Invalid route specification",
        }
    default:
        return APIError{
            Status:  http.StatusInternalServerError,
            Code:    "INTERNAL_ERROR",
            Message: "An internal error occurred",
        }
    }
}
```

### gRPC error mapping

```go
import "google.golang.org/grpc/status"
import "google.golang.org/grpc/codes"

func mapToGRPC(err error) error {
    switch {
    case errors.Is(err, ErrNotFound):
        return status.Error(codes.NotFound, "route not found")
    case errors.Is(err, ErrInvalidRoute):
        return status.Error(codes.InvalidArgument, "invalid route")
    case errors.Is(err, ErrVrfNotExist):
        return status.Error(codes.FailedPrecondition, "VRF not configured")
    default:
        return status.Error(codes.Internal, "internal error")
    }
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Exposing internal errors

```go
// BAD: Exposes internal details
func handler(w http.ResponseWriter, r *http.Request) {
    route, err := service.Get(...)
    if err != nil {
        http.Error(w, err.Error(), 500)  // Leaks internal info!
    }
}

// GOOD: Map to appropriate response
func handler(w http.ResponseWriter, r *http.Request) {
    route, err := service.Get(...)
    if err != nil {
        log.Printf("Error: %v", err)  // Log full error
        apiErr := mapError(err)
        w.WriteHeader(apiErr.Status)
        json.NewEncoder(w).Encode(apiErr)
    }
}
```

### Pitfall 2: Logging at wrong layer

```go
// BAD: Logs at every layer
func (r *Repo) Get() error {
    log.Println("Error in repo")  // Logged here
    return err
}
func (s *Service) Get() error {
    log.Println("Error in service")  // And here
    return err
}
func (h *Handler) Get() {
    log.Println("Error in handler")  // And here
}

// GOOD: Log once at boundary
func (h *Handler) Get(w http.ResponseWriter, r *http.Request) {
    _, err := h.service.Get(...)
    if err != nil {
        log.Printf("request %s: %v", r.URL.Path, err)  // Log once
        // ... respond to client
    }
}
```

---

## 6. Complete Example

```go
package main

import (
    "encoding/json"
    "errors"
    "fmt"
    "log"
    "net/http"
)

// Domain errors
var (
    ErrNotFound    = errors.New("not found")
    ErrInvalidRoute = errors.New("invalid route")
)

type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

// Repository
type RouteRepo struct {
    data map[string]Route
}

func (r *RouteRepo) Get(key string) (Route, error) {
    route, ok := r.data[key]
    if !ok {
        return Route{}, ErrNotFound  // Domain error
    }
    return route, nil
}

// Service
type RouteService struct {
    repo *RouteRepo
}

func (s *RouteService) GetRoute(vrfID uint32, prefix string) (Route, error) {
    key := fmt.Sprintf("%d:%s", vrfID, prefix)
    route, err := s.repo.Get(key)
    if err != nil {
        return Route{}, fmt.Errorf("get route VRF %d prefix %s: %w",
            vrfID, prefix, err)
    }
    return route, nil
}

// API Error
type APIError struct {
    Status  int    `json:"-"`
    Code    string `json:"code"`
    Message string `json:"message"`
}

func mapError(err error) APIError {
    switch {
    case errors.Is(err, ErrNotFound):
        return APIError{
            Status:  http.StatusNotFound,
            Code:    "NOT_FOUND",
            Message: "Route not found",
        }
    case errors.Is(err, ErrInvalidRoute):
        return APIError{
            Status:  http.StatusBadRequest,
            Code:    "INVALID_ROUTE",
            Message: "Invalid route specification",
        }
    default:
        return APIError{
            Status:  http.StatusInternalServerError,
            Code:    "INTERNAL_ERROR",
            Message: "An internal error occurred",
        }
    }
}

// Handler
type RouteHandler struct {
    service *RouteService
}

func (h *RouteHandler) GetRoute(w http.ResponseWriter, r *http.Request) {
    vrfID := uint32(1)  // Parse from request
    prefix := r.URL.Query().Get("prefix")
    
    route, err := h.service.GetRoute(vrfID, prefix)
    if err != nil {
        log.Printf("GetRoute error: %v", err)  // Log full error
        
        apiErr := mapError(err)
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(apiErr.Status)
        json.NewEncoder(w).Encode(apiErr)
        return
    }
    
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(route)
}

func main() {
    repo := &RouteRepo{
        data: map[string]Route{
            "1:10.0.0.0/24": {VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"},
        },
    }
    service := &RouteService{repo: repo}
    handler := &RouteHandler{service: service}
    
    http.HandleFunc("/route", handler.GetRoute)
    log.Println("Server on :8080")
    http.ListenAndServe(":8080", nil)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ERROR BOUNDARY RULES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. DEFINE DOMAIN ERRORS                                               │
│      • Package-level sentinel errors                                    │
│      • Part of public API contract                                      │
│                                                                         │
│   2. TRANSLATE AT BOUNDARIES                                            │
│      • Repository: SQL/infra → domain                                   │
│      • Handler: domain → HTTP/gRPC                                      │
│                                                                         │
│   3. LOG ONCE AT TOP                                                    │
│      • Handler/boundary logs full error                                 │
│      • Lower layers just propagate                                      │
│                                                                         │
│   4. HIDE INTERNAL DETAILS                                              │
│      • Never expose raw errors to clients                               │
│      • Map to appropriate codes/messages                                │
│                                                                         │
│   5. ADD CONTEXT AT EACH LAYER                                          │
│      • Wrap with %w                                                     │
│      • Add layer-specific context                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 错误边界

**错误边界定义了错误应该在哪里处理、在哪里传播。**

### 各层职责

| 层 | 职责 |
|-----|------|
| Handler | 转换为 HTTP/gRPC 响应，日志记录 |
| Service | 添加业务上下文，可能重试 |
| Repository | 翻译 DB 错误为领域错误 |
| Infrastructure | 原始系统错误 |

### 最佳实践

1. **定义领域错误**：包级别哨兵错误
2. **边界处翻译**：SQL → 领域 → HTTP
3. **顶层记录日志**：只在边界记录一次
4. **隐藏内部细节**：不暴露原始错误给客户端
5. **每层添加上下文**：用 `%w` 包装

