# Topic 4: Go Toolchain Overview (go build / go test / go mod)

## 1. Problem It Solves (Engineering Motivation)

Build system fragmentation in other ecosystems:

- **C/C++**: Make, CMake, Autotools, Meson, Ninja, Bazel, Buck...
- **Java**: Ant, Maven, Gradle, Bazel...
- **Python**: setuptools, pip, poetry, pipenv, conda...
- **JavaScript**: npm, yarn, pnpm, webpack, rollup, vite...

Problems:
- Learning curve for each build system
- Configuration file complexity
- Different projects use different tools
- Build reproducibility issues

Go's answer: **One tool to rule them all: `go`**

```
┌────────────────────────────────────────────────────────────────────┐
│                   Build System Comparison                           │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  C++ Project:                    Go Project:                        │
│  ┌──────────────────┐           ┌──────────────────┐               │
│  │ CMakeLists.txt   │           │ go.mod           │               │
│  │ Makefile         │           │ go.sum           │               │
│  │ conanfile.txt    │           │                  │               │
│  │ .clang-format    │           │ (that's it)      │               │
│  │ .clang-tidy      │           │                  │               │
│  │ compile_commands │           └──────────────────┘               │
│  └──────────────────┘                                              │
│                                                                     │
│  Commands needed:                Commands needed:                   │
│  mkdir build && cd build        go build                           │
│  cmake ..                        go test                           │
│  make -j8                        go mod tidy                       │
│  ctest                                                             │
│  make install                                                      │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 提供了一个统一的工具链 `go` 命令，它处理编译、测试、依赖管理、格式化、文档生成等所有任务。不需要学习 Make、CMake 或其他构建系统。所有 Go 项目使用相同的工具，这大大简化了开发流程。

## 2. Core Idea and Mental Model

The `go` command is a **convention-over-configuration** tool:

- **No build files**: Package structure IS the build configuration
- **No dependency config**: Import paths ARE dependency declarations
- **No test config**: Files ending in `_test.go` ARE tests
- **No format config**: `gofmt` IS the format

```
┌─────────────────────────────────────────────────────────────────┐
│                    Go Toolchain Mental Model                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Source Code Structure = Build Configuration                     │
│                                                                  │
│  myapp/                                                          │
│  ├── go.mod              ← Module definition                     │
│  ├── go.sum              ← Dependency checksums                  │
│  ├── main.go             ← package main = executable             │
│  ├── server/                                                     │
│  │   ├── server.go       ← package server                        │
│  │   └── server_test.go  ← tests for server package              │
│  └── client/                                                     │
│      ├── client.go       ← package client                        │
│      └── client_test.go  ← tests for client package              │
│                                                                  │
│  go build ./...    → Builds all packages                         │
│  go test ./...     → Tests all packages                          │
│  go mod tidy       → Updates dependencies                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Core Commands

| Command | Purpose |
|---------|---------|
| `go build` | Compile packages and dependencies |
| `go test` | Run tests |
| `go run` | Compile and run (for development) |
| `go mod` | Module maintenance |
| `go get` | Add dependencies |
| `go fmt` | Format source code |
| `go vet` | Static analysis |
| `go doc` | Show documentation |
| `go generate` | Run code generators |
| `go install` | Compile and install |

### Module System (go.mod)

```go
// go.mod - Module definition
module vecima.com/vcore/routermgr

go 1.21

require (
    github.com/sirupsen/logrus v1.9.0
    google.golang.org/grpc v1.58.0
    vecima.com/vcore/vmc v1.2.0
)

// Replace for local development
replace vecima.com/vcore/vmc => ../vmc
```

### go.sum - Cryptographic Verification

```
github.com/sirupsen/logrus v1.9.0 h1:trlNQbNUG3OdDrDil03MCb1H2o9nJ1x4/5LYw7byDE0=
github.com/sirupsen/logrus v1.9.0/go.mod h1:naHLuLoDiP4jHNo9R0sCBMtWGeIprob74mVsIT4qYEQ=
```

## 4. Typical Real-World Usage

### Project Setup
```bash
# Create new project
mkdir myapp && cd myapp
go mod init github.com/mycompany/myapp

# Add dependencies (automatically added to go.mod)
go get github.com/sirupsen/logrus
go get google.golang.org/grpc

# Clean up unused dependencies
go mod tidy

# Verify dependencies
go mod verify
```

### Development Workflow
```bash
# Format all code (typically in pre-commit hook)
go fmt ./...

# Run static analysis
go vet ./...

# Run tests
go test ./...

# Run tests with coverage
go test -cover ./...

# Run tests with race detector
go test -race ./...

# Build
go build -o myapp ./cmd/myapp

# Build for production
go build -ldflags="-s -w" -o myapp ./cmd/myapp
```

### From Your Codebase Structure

```
routermgr/
├── go.mod
├── go.sum
└── src/
    └── vecima.com/
        └── vcore/
            └── routermgr/
                ├── main.go
                ├── routermgr_grpc.go
                ├── routermgr_mgmtd.go
                └── routermgr_test.go  (if exists)

# Build the binary
cd routermgr
go build -o routermgr ./src/vecima.com/vcore/routermgr

# Test
go test ./src/vecima.com/vcore/routermgr

# Or test everything
go test ./...
```

## 5. Common Mistakes and Pitfalls

1. **Not running `go mod tidy`**:
   ```bash
   # After adding/removing imports, always run:
   go mod tidy
   
   # In CI, verify no changes needed:
   go mod tidy
   git diff --exit-code go.mod go.sum
   ```

2. **Committing vendor without using it**:
   ```bash
   # If you vendor, use it:
   go mod vendor
   go build -mod=vendor ./...
   
   # Or don't vendor at all (most common)
   ```

3. **Ignoring go.sum**:
   ```bash
   # ALWAYS commit go.sum
   # It ensures reproducible builds
   git add go.mod go.sum
   ```

4. **Using GOPATH in modern Go**:
   ```bash
   # Old way (pre-1.11) - GOPATH required
   export GOPATH=/home/user/go
   # Code must be in $GOPATH/src/...
   
   # Modern way (1.11+) - Modules
   # Work anywhere, use go.mod
   ```

5. **Not using `./...` for recursive operations**:
   ```bash
   # Only current package
   go test
   
   # All packages (usually what you want)
   go test ./...
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C/C++ (typical) | Go |
|--------|-----------------|-----|
| Build definition | CMakeLists.txt, Makefile | go.mod (minimal) |
| Dependency fetch | conan, vcpkg, git submodules | `go get` (built-in) |
| Dependency lock | conan.lock, CMake FetchContent | go.sum (built-in) |
| Test runner | CTest, GTest, Catch2 | `go test` (built-in) |
| Formatter | clang-format | `go fmt` (built-in) |
| Static analysis | clang-tidy, cppcheck | `go vet` (built-in) |
| Documentation | Doxygen | `go doc` (built-in) |

### Linux Kernel Comparison
```makefile
# Linux kernel: Complex Makefile system
make menuconfig
make -j$(nproc)
make modules_install
make install

# Go: Just go build
go build
```

## 7. A Small But Complete Go Example

```bash
# Create a complete project from scratch
mkdir example-api && cd example-api

# Initialize module
go mod init github.com/example/api
```

```go
// main.go
package main

import (
    "fmt"
    "log"
    "net/http"

    "github.com/example/api/handler"
)

func main() {
    http.HandleFunc("/", handler.Hello)
    fmt.Println("Server starting on :8080")
    log.Fatal(http.ListenAndServe(":8080", nil))
}
```

```go
// handler/handler.go
package handler

import (
    "fmt"
    "net/http"
)

func Hello(w http.ResponseWriter, r *http.Request) {
    fmt.Fprintf(w, "Hello, World!")
}
```

```go
// handler/handler_test.go
package handler

import (
    "net/http"
    "net/http/httptest"
    "testing"
)

func TestHello(t *testing.T) {
    req := httptest.NewRequest("GET", "/", nil)
    w := httptest.NewRecorder()
    
    Hello(w, req)
    
    if w.Code != http.StatusOK {
        t.Errorf("expected 200, got %d", w.Code)
    }
    if w.Body.String() != "Hello, World!" {
        t.Errorf("unexpected body: %s", w.Body.String())
    }
}
```

```bash
# Full workflow
go mod tidy                    # Update dependencies
go fmt ./...                   # Format code
go vet ./...                   # Static analysis
go test ./...                  # Run tests
go test -cover ./...           # Test with coverage
go build -o api                # Build binary
./api                          # Run

# Output:
# ok      github.com/example/api/handler  0.002s  coverage: 100.0% of statements
# Server starting on :8080
```

### Complete go.mod
```go
module github.com/example/api

go 1.21
```

That's it. No CMakeLists.txt, no Makefile, no package.json, no requirements.txt.

---

**Summary**: The Go toolchain embodies "convention over configuration." The `go` command handles building, testing, formatting, dependency management, documentation, and more. By making the source code structure BE the configuration, Go eliminates build system complexity and ensures every Go project works the same way.

