# Topic 48: Dependency Injection (Go Style)

## 1. Problem It Solves

- Testability (swap real DB for mock)
- Flexibility (change implementations)
- Decoupling (components don't know concrete types)

## 2. Go Style: Constructor Injection

```go
// Define interface at point of use
type UserStore interface {
    Get(id int) (*User, error)
    Save(user *User) error
}

// Service depends on interface
type UserService struct {
    store UserStore
}

// Inject via constructor
func NewUserService(store UserStore) *UserService {
    return &UserService{store: store}
}

// Usage
func main() {
    db := database.New(connStr)
    store := postgres.NewUserStore(db)
    service := NewUserService(store)
}
```

## 3. Testing with Injection

```go
// Mock implementation
type mockUserStore struct {
    users map[int]*User
}

func (m *mockUserStore) Get(id int) (*User, error) {
    if user, ok := m.users[id]; ok {
        return user, nil
    }
    return nil, ErrNotFound
}

// Test
func TestUserService(t *testing.T) {
    mock := &mockUserStore{
        users: map[int]*User{1: {ID: 1, Name: "Test"}},
    }
    service := NewUserService(mock)
    
    user, err := service.GetUser(1)
    // Assert...
}
```

## 4. Wire Pattern (Main Package)

```go
// cmd/server/main.go
func main() {
    // Build dependency graph
    config := loadConfig()
    db := database.New(config.DBUrl)
    
    userStore := postgres.NewUserStore(db)
    userService := service.NewUserService(userStore)
    userHandler := handler.NewUserHandler(userService)
    
    router := setupRouter(userHandler)
    http.ListenAndServe(":8080", router)
}
```

## 5. From routermgr_grpc.go

```go
// Current: global state (harder to test)
var RouterAddresses map[uint32]map[int]RouterAddress

// Better: injected dependencies
type RouterMgrServer struct {
    routermgrpb.UnimplementedRouterMgrServer
    addresses AddressStore
    routes    RouteStore
    mgmtd     MgmtdClient
}

func NewRouterMgrServer(addr AddressStore, routes RouteStore, mgmtd MgmtdClient) *RouterMgrServer {
    return &RouterMgrServer{addresses: addr, routes: routes, mgmtd: mgmtd}
}
```

## 6. No Framework Needed

Go doesn't need DI frameworks:
- Constructor injection is simple
- Interfaces are implicit
- Main package wires everything

---

**Summary**: Use constructor injection with interfaces. Define interfaces where used, not where implemented. Wire dependencies in main(). No framework needed.

