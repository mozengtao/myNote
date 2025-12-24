# net/http: Handler Model and Middleware

## 1. Engineering Problem

### What real-world problem does this solve?

**Go's net/http provides a simple but powerful model for HTTP servers and clients.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    HTTP HANDLER MODEL                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   The Handler Interface:                                                │
│   ──────────────────────                                                │
│                                                                         │
│   type Handler interface {                                              │
│       ServeHTTP(ResponseWriter, *Request)                               │
│   }                                                                     │
│                                                                         │
│   That's it. One method. Everything else builds on this.                │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Request Flow:                                                         │
│                                                                         │
│   Client ──► Server ──► Router ──► Middleware ──► Handler               │
│                            │                          │                 │
│                            ▼                          ▼                 │
│                       /routes/add               Business Logic          │
│                       /routes/del                     │                 │
│                                                       ▼                 │
│                                              Write Response             │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Middleware Pattern:                                                   │
│                                                                         │
│   func middleware(next http.Handler) http.Handler {                     │
│       return http.HandlerFunc(func(w, r) {                             │
│           // Before                                                     │
│           next.ServeHTTP(w, r)                                         │
│           // After                                                      │
│       })                                                                │
│   }                                                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Handler and HandlerFunc

```go
// Handler is the interface
type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}

// HandlerFunc is an adapter
type HandlerFunc func(ResponseWriter, *Request)

func (f HandlerFunc) ServeHTTP(w ResponseWriter, r *Request) {
    f(w, r)
}

// Any function with right signature can be a handler
http.Handle("/path", http.HandlerFunc(myFunc))
http.HandleFunc("/path", myFunc)  // Shorthand
```

### Request lifecycle

```go
func handler(w http.ResponseWriter, r *http.Request) {
    // 1. Read request
    body, _ := io.ReadAll(r.Body)
    defer r.Body.Close()
    
    // 2. Process
    result := process(body)
    
    // 3. Write response
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusOK)
    json.NewEncoder(w).Encode(result)
}
```

---

## 3. Language Mechanism

### Basic server

```go
func main() {
    http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
        w.WriteHeader(http.StatusOK)
        w.Write([]byte("OK"))
    })
    
    log.Fatal(http.ListenAndServe(":8080", nil))
}
```

### Custom ServeMux

```go
func main() {
    mux := http.NewServeMux()
    
    mux.HandleFunc("/routes", routeHandler)
    mux.HandleFunc("/routes/", routeDetailHandler)
    
    server := &http.Server{
        Addr:         ":8080",
        Handler:      mux,
        ReadTimeout:  10 * time.Second,
        WriteTimeout: 10 * time.Second,
    }
    
    log.Fatal(server.ListenAndServe())
}
```

### Middleware pattern

```go
func logging(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        next.ServeHTTP(w, r)
        log.Printf("%s %s %v", r.Method, r.URL.Path, time.Since(start))
    })
}

func auth(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        token := r.Header.Get("Authorization")
        if !isValid(token) {
            http.Error(w, "Unauthorized", http.StatusUnauthorized)
            return
        }
        next.ServeHTTP(w, r)
    })
}

// Chain middleware
handler := logging(auth(routeHandler))
```

### Context in requests

```go
func handler(w http.ResponseWriter, r *http.Request) {
    ctx := r.Context()
    
    // Set deadline
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()
    
    // Pass to downstream
    result, err := db.QueryContext(ctx, "SELECT ...")
    if err != nil {
        if ctx.Err() == context.DeadlineExceeded {
            http.Error(w, "Timeout", http.StatusGatewayTimeout)
            return
        }
        http.Error(w, "Error", http.StatusInternalServerError)
        return
    }
    
    json.NewEncoder(w).Encode(result)
}
```

---

## 4. Idiomatic Usage

### Graceful shutdown

```go
func main() {
    server := &http.Server{Addr: ":8080", Handler: mux}
    
    go func() {
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            log.Fatalf("Server error: %v", err)
        }
    }()
    
    // Wait for interrupt
    stop := make(chan os.Signal, 1)
    signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
    <-stop
    
    // Graceful shutdown
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    
    if err := server.Shutdown(ctx); err != nil {
        log.Printf("Shutdown error: %v", err)
    }
}
```

### Structured handler

```go
type RouteHandler struct {
    store *RouteStore
    log   *log.Logger
}

func (h *RouteHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    switch r.Method {
    case http.MethodGet:
        h.handleGet(w, r)
    case http.MethodPost:
        h.handlePost(w, r)
    default:
        http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
    }
}

func (h *RouteHandler) handleGet(w http.ResponseWriter, r *http.Request) {
    routes := h.store.List()
    json.NewEncoder(w).Encode(routes)
}

func (h *RouteHandler) handlePost(w http.ResponseWriter, r *http.Request) {
    var route Route
    if err := json.NewDecoder(r.Body).Decode(&route); err != nil {
        http.Error(w, err.Error(), http.StatusBadRequest)
        return
    }
    
    if err := h.store.Add(route); err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    
    w.WriteHeader(http.StatusCreated)
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Writing after WriteHeader

```go
// BAD: Header already sent
func handler(w http.ResponseWriter, r *http.Request) {
    w.WriteHeader(http.StatusOK)
    w.Header().Set("X-Custom", "value")  // Too late!
}

// GOOD: Set headers first
func handler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("X-Custom", "value")
    w.WriteHeader(http.StatusOK)
}
```

### Pitfall 2: Not closing request body

```go
// BAD: Body not closed
func handler(w http.ResponseWriter, r *http.Request) {
    data, _ := io.ReadAll(r.Body)
    // r.Body not closed!
}

// GOOD: Always close
func handler(w http.ResponseWriter, r *http.Request) {
    defer r.Body.Close()
    data, _ := io.ReadAll(r.Body)
}
```

### Pitfall 3: Ignoring context cancellation

```go
// BAD: Ignores client disconnect
func handler(w http.ResponseWriter, r *http.Request) {
    time.Sleep(10 * time.Second)  // Client may have left
    w.Write([]byte("done"))
}

// GOOD: Respect context
func handler(w http.ResponseWriter, r *http.Request) {
    select {
    case <-time.After(10 * time.Second):
        w.Write([]byte("done"))
    case <-r.Context().Done():
        return  // Client disconnected
    }
}
```

---

## 6. Complete Example

```go
package main

import (
    "context"
    "encoding/json"
    "log"
    "net/http"
    "os"
    "os/signal"
    "sync"
    "syscall"
    "time"
)

type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

type RouteStore struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewRouteStore() *RouteStore {
    return &RouteStore{routes: make(map[string]Route)}
}

func (s *RouteStore) Add(r Route) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.routes[r.Prefix] = r
}

func (s *RouteStore) List() []Route {
    s.mu.RLock()
    defer s.mu.RUnlock()
    result := make([]Route, 0, len(s.routes))
    for _, r := range s.routes {
        result = append(result, r)
    }
    return result
}

// Middleware: Logging
func logging(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        next.ServeHTTP(w, r)
        log.Printf("%s %s %v", r.Method, r.URL.Path, time.Since(start))
    })
}

// Middleware: Recovery
func recovery(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("panic: %v", err)
                http.Error(w, "Internal Error", http.StatusInternalServerError)
            }
        }()
        next.ServeHTTP(w, r)
    })
}

// Middleware: Request ID
func requestID(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        id := r.Header.Get("X-Request-ID")
        if id == "" {
            id = generateID()
        }
        ctx := context.WithValue(r.Context(), "request_id", id)
        w.Header().Set("X-Request-ID", id)
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

func generateID() string {
    return time.Now().Format("20060102150405.000000")
}

// Handler
type RouteHandler struct {
    store *RouteStore
}

func (h *RouteHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    switch r.Method {
    case http.MethodGet:
        h.list(w, r)
    case http.MethodPost:
        h.add(w, r)
    default:
        http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
    }
}

func (h *RouteHandler) list(w http.ResponseWriter, r *http.Request) {
    routes := h.store.List()
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(routes)
}

func (h *RouteHandler) add(w http.ResponseWriter, r *http.Request) {
    defer r.Body.Close()
    
    var route Route
    if err := json.NewDecoder(r.Body).Decode(&route); err != nil {
        http.Error(w, "Invalid JSON", http.StatusBadRequest)
        return
    }
    
    if route.Prefix == "" {
        http.Error(w, "prefix required", http.StatusBadRequest)
        return
    }
    
    h.store.Add(route)
    
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusCreated)
    json.NewEncoder(w).Encode(route)
}

func main() {
    store := NewRouteStore()
    
    mux := http.NewServeMux()
    mux.Handle("/routes", &RouteHandler{store: store})
    mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
        w.Write([]byte("OK"))
    })
    
    // Chain middleware
    handler := logging(recovery(requestID(mux)))
    
    server := &http.Server{
        Addr:         ":8080",
        Handler:      handler,
        ReadTimeout:  10 * time.Second,
        WriteTimeout: 10 * time.Second,
        IdleTimeout:  60 * time.Second,
    }
    
    // Start server
    go func() {
        log.Println("Server starting on :8080")
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            log.Fatalf("Server error: %v", err)
        }
    }()
    
    // Graceful shutdown
    stop := make(chan os.Signal, 1)
    signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
    <-stop
    
    log.Println("Shutting down...")
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    
    if err := server.Shutdown(ctx); err != nil {
        log.Printf("Shutdown error: %v", err)
    }
    log.Println("Server stopped")
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    HTTP DESIGN RULES                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE MIDDLEWARE FOR CROSS-CUTTING CONCERNS                          │
│      • Logging, auth, recovery, tracing                                 │
│      • Chain: logging(auth(handler))                                    │
│                                                                         │
│   2. RESPECT REQUEST CONTEXT                                            │
│      • Use r.Context() for cancellation                                 │
│      • Pass to downstream calls                                         │
│                                                                         │
│   3. ALWAYS CLOSE REQUEST BODY                                          │
│      • defer r.Body.Close()                                             │
│                                                                         │
│   4. SET HEADERS BEFORE WriteHeader                                     │
│      • Headers must be set first                                        │
│      • Write() implicitly calls WriteHeader                             │
│                                                                         │
│   5. IMPLEMENT GRACEFUL SHUTDOWN                                        │
│      • Catch signals                                                    │
│      • server.Shutdown(ctx)                                             │
│                                                                         │
│   6. SET TIMEOUTS                                                       │
│      • ReadTimeout, WriteTimeout, IdleTimeout                           │
│      • Prevent slow clients from exhausting resources                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### Handler 接口

```go
type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}
```

任何实现此接口的类型都可以处理 HTTP 请求。

### 中间件模式

```go
func middleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w, r) {
        // 前置处理
        next.ServeHTTP(w, r)
        // 后置处理
    })
}
```

### 常见中间件

| 中间件 | 用途 |
|--------|------|
| logging | 记录请求日志 |
| recovery | 捕获 panic |
| auth | 认证授权 |
| requestID | 请求追踪 |

### 优雅关闭

1. 监听系统信号（SIGINT, SIGTERM）
2. 调用 `server.Shutdown(ctx)`
3. 等待现有请求完成
4. 超时后强制关闭

### 最佳实践

- 使用中间件处理横切关注点
- 尊重请求上下文
- 始终关闭请求体
- 在 WriteHeader 前设置头
- 实现优雅关闭
- 设置超时

