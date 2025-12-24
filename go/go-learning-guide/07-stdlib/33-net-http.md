# Topic 33: net/http Architecture and Handler Model

## 1. Problem It Solves

Building HTTP servers and clients without external dependencies.

## 2. Core Concepts

### Handler Interface

```go
type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}

// HandlerFunc adapter
type HandlerFunc func(ResponseWriter, *Request)

func (f HandlerFunc) ServeHTTP(w ResponseWriter, r *Request) {
    f(w, r)
}
```

### Basic Server

```go
func main() {
    http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
        fmt.Fprintf(w, "Hello, %s!", r.URL.Path[1:])
    })
    
    log.Fatal(http.ListenAndServe(":8080", nil))
}
```

### Custom Mux

```go
mux := http.NewServeMux()
mux.HandleFunc("/api/", apiHandler)
mux.HandleFunc("/health", healthHandler)

server := &http.Server{
    Addr:         ":8080",
    Handler:      mux,
    ReadTimeout:  10 * time.Second,
    WriteTimeout: 10 * time.Second,
}

log.Fatal(server.ListenAndServe())
```

## 3. Middleware Pattern

```go
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        next.ServeHTTP(w, r)
        log.Printf("%s %s %v", r.Method, r.URL.Path, time.Since(start))
    })
}

// Usage
handler := loggingMiddleware(http.HandlerFunc(myHandler))
```

## 4. Graceful Shutdown

```go
server := &http.Server{Addr: ":8080", Handler: mux}

go func() {
    if err := server.ListenAndServe(); err != http.ErrServerClosed {
        log.Fatal(err)
    }
}()

// Wait for interrupt
quit := make(chan os.Signal, 1)
signal.Notify(quit, os.Interrupt)
<-quit

ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()
server.Shutdown(ctx)
```

## 5. HTTP Client

```go
client := &http.Client{
    Timeout: 10 * time.Second,
}

resp, err := client.Get("https://api.example.com/data")
if err != nil {
    return err
}
defer resp.Body.Close()

body, err := io.ReadAll(resp.Body)
```

---

**Summary**: net/http provides a complete HTTP implementation. Use Handler interface for extensibility, middleware for cross-cutting concerns, and always configure timeouts.

