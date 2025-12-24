# Topic 50: Layered vs Package-Oriented Architecture

## 1. Layered Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Layered Architecture                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  cmd/server/main.go    (entry point, wiring)                     │
│         │                                                        │
│         ▼                                                        │
│  internal/handler/     (HTTP/gRPC handlers)                      │
│         │                                                        │
│         ▼                                                        │
│  internal/service/     (business logic)                          │
│         │                                                        │
│         ▼                                                        │
│  internal/repository/  (data access)                             │
│         │                                                        │
│         ▼                                                        │
│  internal/model/       (domain types)                            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

```
myapp/
├── cmd/server/main.go
├── internal/
│   ├── handler/
│   │   └── user.go
│   ├── service/
│   │   └── user.go
│   ├── repository/
│   │   └── user.go
│   └── model/
│       └── user.go
```

## 2. Package-Oriented (Domain-Driven)

```
myapp/
├── cmd/server/main.go
├── user/                  # Everything about users
│   ├── user.go           # Domain type
│   ├── service.go        # Business logic
│   ├── repository.go     # Data access
│   └── handler.go        # HTTP handlers
├── order/                 # Everything about orders
│   ├── order.go
│   ├── service.go
│   └── handler.go
└── pkg/                   # Shared utilities
    └── database/
```

## 3. When to Use Which

### Layered: Traditional applications
- Clear separation of concerns
- Easy to understand
- Works well for CRUD apps

### Package-Oriented: Microservices
- Better encapsulation
- Domain-driven design
- Easier to split into services later

## 4. Hybrid Approach (Common)

```
myapp/
├── cmd/server/main.go
├── internal/
│   ├── user/
│   │   ├── handler.go
│   │   ├── service.go
│   │   └── store.go
│   ├── order/
│   │   └── ...
│   └── platform/          # Shared infrastructure
│       ├── database/
│       └── auth/
└── pkg/                   # Public API (if any)
```

## 5. routermgr Structure

Current (flat):
```
routermgr/
└── src/vecima.com/vcore/routermgr/
    ├── main.go
    ├── routermgr_grpc.go    # gRPC handlers
    └── routermgr_mgmtd.go   # External API
```

Could evolve to:
```
routermgr/
├── cmd/routermgr/main.go
├── internal/
│   ├── grpc/
│   │   └── handler.go
│   ├── mgmtd/
│   │   └── client.go
│   └── store/
│       ├── address.go
│       └── route.go
```

---

**Summary**: Start simple (flat or layered). Add structure as needed. Packages should have clear, single purposes. Dependencies flow inward.

