# Signal Handling: Graceful Shutdown

## 1. Engineering Problem

### What real-world problem does this solve?

**Long-running services must shut down gracefully to avoid data loss and connection errors.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GRACEFUL SHUTDOWN                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Abrupt shutdown:                   Graceful shutdown:                 │
│   ────────────────                   ──────────────────                 │
│                                                                         │
│   SIGKILL received                   SIGTERM received                   │
│        │                                  │                             │
│        ▼                                  ▼                             │
│   Process killed                     Stop accepting new                 │
│   immediately                        connections                        │
│        │                                  │                             │
│        ▼                                  ▼                             │
│   • In-flight requests lost          Wait for in-flight                 │
│   • Connections dropped              requests to complete               │
│   • Data may be corrupted                 │                             │
│   • Clients see errors                    ▼                             │
│                                      Close resources                    │
│                                      (DB, files, etc.)                  │
│                                           │                             │
│                                           ▼                             │
│                                      Exit cleanly                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- In-flight requests get dropped when container restarts
- Database connections left in bad state
- Clients receive connection reset errors
- Data corruption from interrupted writes
- Resource leaks (file handles, sockets)

---

## 2. Core Mental Model

### How Go expects you to think

**Catch signals, trigger controlled shutdown, wait for completion with timeout.**

```go
// 1. Create signal channel
stop := make(chan os.Signal, 1)
signal.Notify(stop, os.Interrupt, syscall.SIGTERM)

// 2. Block until signal
<-stop

// 3. Graceful shutdown with timeout
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()
server.Shutdown(ctx)
```

### Common signals

| Signal | Meaning | Default Behavior |
|--------|---------|------------------|
| SIGINT | Interrupt (Ctrl+C) | Terminate |
| SIGTERM | Termination request | Terminate |
| SIGKILL | Force kill | Cannot catch |
| SIGHUP | Hangup | Terminate (often reload) |

### Philosophy

- Give in-flight work time to complete
- Set hard deadline to avoid hanging
- Clean up resources explicitly
- Log shutdown progress

---

## 3. Language Mechanism

### signal.Notify

```go
import (
    "os"
    "os/signal"
    "syscall"
)

// Create buffered channel (MUST be buffered)
stop := make(chan os.Signal, 1)

// Register for signals
signal.Notify(stop,
    os.Interrupt,    // SIGINT
    syscall.SIGTERM, // SIGTERM
)

// Block until signal received
sig := <-stop
fmt.Printf("Received signal: %v\n", sig)
```

### http.Server.Shutdown

```go
server := &http.Server{Addr: ":8080", Handler: mux}

// Start server in goroutine
go func() {
    if err := server.ListenAndServe(); err != http.ErrServerClosed {
        log.Fatalf("Server error: %v", err)
    }
}()

// Wait for signal
<-stop

// Graceful shutdown (waits for connections to close)
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()

if err := server.Shutdown(ctx); err != nil {
    log.Printf("Shutdown error: %v", err)
}
```

### grpc.Server.GracefulStop

```go
grpcServer := grpc.NewServer()
// Register services...

go func() {
    lis, _ := net.Listen("tcp", ":50051")
    grpcServer.Serve(lis)
}()

<-stop

// GracefulStop waits for RPCs to complete
grpcServer.GracefulStop()
```

---

## 4. Idiomatic Usage

### When to implement graceful shutdown

- HTTP/gRPC servers
- Message queue consumers
- Background workers
- Any long-running service

### Complete server lifecycle

```go
func main() {
    // 1. Setup
    server := setupServer()
    
    // 2. Start
    go server.Start()
    
    // 3. Wait for shutdown signal
    waitForShutdown()
    
    // 4. Graceful shutdown
    server.Shutdown()
}
```

### Pattern: Shutdown coordinator

```go
type App struct {
    server     *http.Server
    grpcServer *grpc.Server
    db         *sql.DB
    done       chan struct{}
}

func (a *App) Shutdown(ctx context.Context) error {
    // Shutdown in order
    if err := a.server.Shutdown(ctx); err != nil {
        return err
    }
    
    a.grpcServer.GracefulStop()
    
    if err := a.db.Close(); err != nil {
        return err
    }
    
    close(a.done)
    return nil
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Unbuffered signal channel

```go
// BAD: Unbuffered channel, signal may be lost
stop := make(chan os.Signal)  // No buffer!
signal.Notify(stop, os.Interrupt)

// GOOD: Buffered channel
stop := make(chan os.Signal, 1)  // Buffer of 1
signal.Notify(stop, os.Interrupt)
```

### Pitfall 2: No shutdown timeout

```go
// BAD: May hang forever
<-stop
server.Shutdown(context.Background())  // No timeout!

// GOOD: Timeout ensures eventual exit
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()
if err := server.Shutdown(ctx); err != nil {
    log.Printf("Forced shutdown: %v", err)
}
```

### Pitfall 3: Resources not closed

```go
// BAD: Database connection leaked
func main() {
    db, _ := sql.Open("postgres", dsn)
    // ... server runs ...
    // db never closed!
}

// GOOD: Explicit cleanup
func main() {
    db, _ := sql.Open("postgres", dsn)
    defer db.Close()
    
    // ... server runs ...
    
    <-stop
    // db.Close() called by defer
}
```

### Pitfall 4: Ignoring shutdown errors

```go
// BAD: Errors ignored
server.Shutdown(ctx)

// GOOD: Log shutdown errors
if err := server.Shutdown(ctx); err != nil {
    log.Printf("Shutdown error: %v", err)
    os.Exit(1)
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "context"
    "encoding/json"
    "fmt"
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

type RouteManager struct {
    mu     sync.RWMutex
    routes map[string]Route
}

func NewRouteManager() *RouteManager {
    return &RouteManager{routes: make(map[string]Route)}
}

func (rm *RouteManager) Add(r Route) {
    rm.mu.Lock()
    defer rm.mu.Unlock()
    rm.routes[r.Prefix] = r
}

func (rm *RouteManager) List() []Route {
    rm.mu.RLock()
    defer rm.mu.RUnlock()
    routes := make([]Route, 0, len(rm.routes))
    for _, r := range rm.routes {
        routes = append(routes, r)
    }
    return routes
}

type Server struct {
    httpServer   *http.Server
    routeManager *RouteManager
    done         chan struct{}
}

func NewServer(addr string, rm *RouteManager) *Server {
    s := &Server{
        routeManager: rm,
        done:         make(chan struct{}),
    }
    
    mux := http.NewServeMux()
    mux.HandleFunc("/routes", s.handleRoutes)
    mux.HandleFunc("/health", s.handleHealth)
    
    s.httpServer = &http.Server{
        Addr:         addr,
        Handler:      mux,
        ReadTimeout:  10 * time.Second,
        WriteTimeout: 10 * time.Second,
    }
    
    return s
}

func (s *Server) handleRoutes(w http.ResponseWriter, r *http.Request) {
    // Simulate slow request
    time.Sleep(100 * time.Millisecond)
    
    routes := s.routeManager.List()
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(routes)
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
    w.WriteHeader(http.StatusOK)
    w.Write([]byte("OK"))
}

func (s *Server) Start() error {
    log.Printf("Starting server on %s", s.httpServer.Addr)
    if err := s.httpServer.ListenAndServe(); err != http.ErrServerClosed {
        return err
    }
    return nil
}

func (s *Server) Shutdown(ctx context.Context) error {
    log.Println("Shutting down server...")
    
    // Shutdown HTTP server (waits for in-flight requests)
    if err := s.httpServer.Shutdown(ctx); err != nil {
        return fmt.Errorf("http shutdown: %w", err)
    }
    
    log.Println("Server stopped")
    close(s.done)
    return nil
}

func (s *Server) Done() <-chan struct{} {
    return s.done
}

func main() {
    // Initialize
    rm := NewRouteManager()
    rm.Add(Route{VrfID: 1, Prefix: "10.0.0.0/24", NextHop: "192.168.1.1"})
    rm.Add(Route{VrfID: 1, Prefix: "10.0.1.0/24", NextHop: "192.168.1.2"})
    
    server := NewServer(":8080", rm)
    
    // Start server in goroutine
    go func() {
        if err := server.Start(); err != nil {
            log.Printf("Server error: %v", err)
        }
    }()
    
    // Setup signal handling
    stop := make(chan os.Signal, 1)
    signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
    
    log.Println("Server started. Press Ctrl+C to stop.")
    
    // Wait for signal
    sig := <-stop
    log.Printf("Received signal: %v", sig)
    
    // Graceful shutdown with timeout
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    
    if err := server.Shutdown(ctx); err != nil {
        log.Printf("Shutdown error: %v", err)
        os.Exit(1)
    }
    
    // Wait for shutdown to complete
    <-server.Done()
    log.Println("Shutdown complete")
}
```

Run and test:
```bash
# Start server
go run main.go

# In another terminal, make request while shutting down
curl http://localhost:8080/routes &
kill -SIGTERM $(pgrep -f main)

# Request should complete before server exits
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SIGNAL HANDLING RULES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE BUFFERED SIGNAL CHANNEL                                        │
│      • make(chan os.Signal, 1)                                          │
│      • Unbuffered may lose signal                                       │
│                                                                         │
│   2. CATCH SIGINT AND SIGTERM                                           │
│      • SIGINT: Ctrl+C                                                   │
│      • SIGTERM: Container orchestrator                                  │
│      • SIGKILL cannot be caught                                         │
│                                                                         │
│   3. SHUTDOWN WITH TIMEOUT                                              │
│      • Give in-flight requests time                                     │
│      • Force exit if timeout exceeded                                   │
│      • 30 seconds is typical                                            │
│                                                                         │
│   4. CLOSE RESOURCES IN ORDER                                           │
│      • Stop accepting new work                                          │
│      • Wait for in-flight work                                          │
│      • Close databases, files, etc.                                     │
│                                                                         │
│   5. LOG SHUTDOWN PROGRESS                                              │
│      • "Received signal"                                                │
│      • "Shutting down..."                                               │
│      • "Shutdown complete"                                              │
│                                                                         │
│   6. USE server.Shutdown FOR HTTP                                       │
│      • Waits for connections to close                                   │
│      • Returns when all done or timeout                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### 优雅关闭流程

```
收到信号 → 停止接受新连接 → 等待进行中的请求 → 关闭资源 → 退出
```

### 关键 API

```go
// 注册信号
signal.Notify(stop, os.Interrupt, syscall.SIGTERM)

// HTTP 服务器
server.Shutdown(ctx)  // 等待连接关闭

// gRPC 服务器
grpcServer.GracefulStop()  // 等待 RPC 完成
```

### 常见陷阱

| 陷阱 | 问题 | 解决方案 |
|------|------|----------|
| 无缓冲通道 | 信号丢失 | `make(chan os.Signal, 1)` |
| 无超时 | 永久挂起 | `WithTimeout(ctx, 30*time.Second)` |
| 不关闭资源 | 资源泄漏 | 显式关闭 DB、文件等 |

### 最佳实践

1. 用缓冲信号通道
2. 捕获 SIGINT 和 SIGTERM
3. 设置关闭超时
4. 按顺序关闭资源
5. 记录关闭进度
