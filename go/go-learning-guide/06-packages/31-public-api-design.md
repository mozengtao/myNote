# Topic 31: Public API Design in Go

## 1. Problem It Solves

How to design APIs that are:
- Easy to use correctly
- Hard to use incorrectly
- Stable over time
- Testable and mockable

## 2. API Design Principles

### Accept Interfaces, Return Structs

```go
// Good: accept interface
func Process(r io.Reader) error {
    // Works with files, buffers, network, etc.
}

// Return concrete type
func NewClient(addr string) *Client {
    return &Client{addr: addr}
}
```

### Small Interfaces

```go
// Good: single-method interface
type Stringer interface {
    String() string
}

// Bad: large interface
type DoEverything interface {
    Read()
    Write()
    Close()
    Process()
    Validate()
    // ...
}
```

### Functional Options Pattern

```go
type Server struct {
    addr    string
    timeout time.Duration
    logger  *log.Logger
}

type Option func(*Server)

func WithTimeout(d time.Duration) Option {
    return func(s *Server) { s.timeout = d }
}

func WithLogger(l *log.Logger) Option {
    return func(s *Server) { s.logger = l }
}

func NewServer(addr string, opts ...Option) *Server {
    s := &Server{addr: addr, timeout: 30 * time.Second}
    for _, opt := range opts {
        opt(s)
    }
    return s
}

// Usage
srv := NewServer(":8080",
    WithTimeout(60*time.Second),
    WithLogger(myLogger),
)
```

## 3. From routermgr_grpc.go

```go
// gRPC defines the interface (in .proto file)
// Implementation is concrete struct
type routermgrServer struct {
    routermgrpb.UnimplementedRouterMgrServer
}

// Methods follow interface contract
func (s *routermgrServer) AddRouteV4(ctx context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error)
```

## 4. Best Practices

1. **Export minimally**: Only export what users need
2. **Document exported types**: Godoc comments on all public APIs
3. **Use consistent naming**: Get/Set, New, Must prefixes
4. **Avoid breaking changes**: Once exported, it's part of your API

---

**Summary**: Accept interfaces, return concrete types. Use functional options for configuration. Export minimally. Document everything public.

