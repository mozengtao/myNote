# Topic 29: Go Modules and Dependency Management

## 1. go.mod File

```go
module vecima.com/vcore/routermgr

go 1.21

require (
    github.com/sirupsen/logrus v1.9.0
    google.golang.org/grpc v1.58.0
)

replace vecima.com/vcore/vmc => ../vmc  // Local development
```

## 2. Essential Commands

```bash
# Initialize new module
go mod init github.com/user/myproject

# Add dependencies
go get github.com/sirupsen/logrus@v1.9.0

# Update go.mod and go.sum
go mod tidy

# Download dependencies
go mod download

# Vendor dependencies
go mod vendor
```

## 3. go.sum

Cryptographic checksums for reproducible builds:
```
github.com/sirupsen/logrus v1.9.0 h1:trlNQbNUG3...
github.com/sirupsen/logrus v1.9.0/go.mod h1:naHLuL...
```

## 4. Version Selection

```bash
go get pkg@v1.2.3      # Specific version
go get pkg@latest      # Latest version
go get pkg@master      # Branch/commit
```

---

**Summary**: go.mod defines your module and dependencies. Use `go mod tidy` frequently. Commit both go.mod and go.sum.

