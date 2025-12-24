# Topic 30: Idiomatic Project Layout

## Standard Layout

```
myproject/
├── go.mod
├── go.sum
├── main.go           # Or in cmd/
│
├── cmd/              # Executables
│   ├── server/
│   │   └── main.go
│   └── cli/
│       └── main.go
│
├── internal/         # Private packages
│   ├── auth/
│   ├── database/
│   └── handler/
│
├── pkg/              # Public libraries
│   └── client/
│
├── api/              # API definitions (proto, OpenAPI)
│   └── v1/
│
└── scripts/          # Build/deploy scripts
```

## Guidelines

1. **Start simple**: Don't over-engineer initially
2. **cmd/**: One directory per binary
3. **internal/**: Private code
4. **pkg/**: Public, reusable code (optional)
5. **Flat is OK**: Small projects don't need deep nesting

## Your routermgr Layout

```
routermgr/
├── go.mod
└── src/vecima.com/vcore/routermgr/
    ├── main.go
    ├── routermgr_grpc.go
    └── routermgr_mgmtd.go
```

---

**Summary**: Start flat, add structure as needed. Use `internal/` for private code, `cmd/` for binaries.

