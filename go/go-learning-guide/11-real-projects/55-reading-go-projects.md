# Topic 55: How to Read Go Open-Source Projects

## 1. Start With These

### Standard Library
```bash
# Find and read
go doc net/http
go doc -src net/http.HandlerFunc
```

Best files to read:
- `src/sync/mutex.go` - Simple, well-documented
- `src/io/io.go` - Interface definitions
- `src/net/http/server.go` - Real server code

### Popular Projects
- **Docker CLI**: `github.com/docker/cli`
- **Hugo**: `github.com/gohugoio/hugo`
- **Prometheus**: `github.com/prometheus/prometheus`
- **CockroachDB**: `github.com/cockroachdb/cockroach`

## 2. How to Approach

### Step 1: Understand Structure
```bash
tree -L 2 -d  # Directory structure
cat go.mod   # Dependencies
```

### Step 2: Find Entry Point
```bash
# For executables
cat cmd/*/main.go

# For libraries
cat *.go | head -50  # Package doc
```

### Step 3: Trace a Feature
1. Pick a feature (e.g., "how does HTTP serving work")
2. Find the entry function
3. Follow the call chain
4. Read tests for usage examples

## 3. Tools for Reading

```bash
# Find definitions
go doc package.Function
go doc -src package.Function

# Find usages
grep -r "FunctionName" .

# Generate documentation
godoc -http=:6060
# Visit http://localhost:6060
```

## 4. Reading Patterns

### Entry Points
```go
// Look for main()
func main() {
    // Initialization
    // Run server
}

// Look for New* functions
func NewServer(config Config) *Server { }
```

### Test Files
```go
// Tests show how to use the code
func TestHandler(t *testing.T) {
    srv := NewServer(cfg)
    // Usage example
}
```

### Interface Implementations
```bash
# Find what implements an interface
grep -r "func.*Method(" .
```

## 5. Recommended Reading Order

1. **sync/mutex.go** - Simple, well-commented
2. **io/io.go** - Core interfaces
3. **net/http/server.go** - Real server code
4. **encoding/json/encode.go** - Reflection use
5. **context/context.go** - Context implementation

---

**Summary**: Start with stdlib. Use `go doc -src`. Read tests for examples. Follow call chains from main() or New*() functions.

