# Topic 28: Package System and Visibility Rules

## 1. Problem It Solves

- Code organization
- Encapsulation and information hiding
- Dependency management
- API surface control

## 2. Package Basics

```go
// Every Go file starts with package declaration
package main      // Executable
package router    // Library

// Import packages
import (
    "fmt"                           // Standard library
    "github.com/user/repo/pkg"      // External
    "myapp/internal/auth"           // Internal
)
```

## 3. Visibility Rules

**Capitalization determines visibility**:

```go
package user

// Exported (public) - capital letter
type User struct {
    ID   int    // Exported
    Name string // Exported
    age  int    // Unexported
}

func NewUser() *User { }  // Exported
func validate() bool { }  // Unexported
```

## 4. From routermgr_grpc.go

```go
package main  // Executable

// Exported types (can be used if this were a library)
type RouterAddress struct {
    VrfId     uint32   // Exported
    Address   string   // Exported
}

// Unexported - package internal
var addressMutex sync.Mutex  // lowercase = private

// Exported function
func StartGrpcServer() { }
```

## 5. Internal Packages

```
myapp/
├── internal/       # Can only be imported by myapp/*
│   └── auth/
├── pkg/            # Public API
│   └── client/
└── cmd/
    └── server/
```

---

**Summary**: Packages organize code. Capitalization controls visibility. Use `internal/` for private packages.

