# Topic 49: Interface-Driven Design

## 1. Core Principle

> "Accept interfaces, return structs"

Define interfaces at the point of use, not at the point of implementation.

## 2. Consumer-Defined Interfaces

```go
// ❌ Java style: implementer defines interface
package storage

type StorageInterface interface {
    Get(key string) ([]byte, error)
    Put(key string, value []byte) error
    Delete(key string) error
    List(prefix string) ([]string, error)
    // ...20 more methods
}

type S3Storage struct{}
func (s *S3Storage) Get(...) {...}
// Implements StorageInterface

// ✅ Go style: consumer defines interface
package myservice

type KeyValueGetter interface {
    Get(key string) ([]byte, error)
}

func ProcessData(kv KeyValueGetter) error {
    data, err := kv.Get("mykey")
    // ...
}
// S3Storage automatically satisfies KeyValueGetter!
```

## 3. Small Interfaces

```go
// Standard library examples
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

type Closer interface {
    Close() error
}

// Compose when needed
type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}
```

## 4. Interface Segregation

```go
// ❌ Fat interface
type Repository interface {
    Create(entity Entity) error
    Read(id string) (Entity, error)
    Update(entity Entity) error
    Delete(id string) error
    List(filter Filter) ([]Entity, error)
    Count(filter Filter) (int, error)
    // etc.
}

// ✅ Segregated interfaces
type EntityReader interface {
    Read(id string) (Entity, error)
}

type EntityWriter interface {
    Create(entity Entity) error
    Update(entity Entity) error
}

// Consumers request only what they need
func LoadEntity(r EntityReader, id string) {...}
```

## 5. Testing Benefits

```go
// Easy to mock small interfaces
type TimeProvider interface {
    Now() time.Time
}

// In production
type realTime struct{}
func (realTime) Now() time.Time { return time.Now() }

// In tests
type fixedTime struct{ t time.Time }
func (f fixedTime) Now() time.Time { return f.t }

func TestExpiry(t *testing.T) {
    fixed := fixedTime{time.Date(2024, 1, 1, 0, 0, 0, 0, time.UTC)}
    service := NewService(fixed)
    // Test with controlled time
}
```

## 6. From routermgr_grpc.go

```go
// The gRPC interface is defined in protobuf
// Go generates the interface, we implement it
type RouterMgrServer interface {
    AddRouteV4(context.Context, *AddIpv4Route) (*RouteActionResponse, error)
    DeleteRouteV4(context.Context, *DeleteIpv4Route) (*RouteActionResponse, error)
    // ...
}

// Our implementation
type routermgrServer struct {
    routermgrpb.UnimplementedRouterMgrServer
}

func (s *routermgrServer) AddRouteV4(...) {...}
```

---

**Summary**: Define small interfaces where used. Let implementations satisfy them implicitly. This creates loosely coupled, testable code.

