# Topic 56: Recommended Learning Projects (Small → Real)

## Level 1: Basics (Days 1-3)

### Project: CLI Tool
```bash
# Build a simple CLI tool
myapp --flag value input.txt
```

Learn:
- Package structure
- Flag parsing
- File I/O
- Error handling

```go
package main

import (
    "flag"
    "fmt"
    "os"
)

func main() {
    verbose := flag.Bool("v", false, "verbose output")
    flag.Parse()
    
    if flag.NArg() < 1 {
        fmt.Fprintln(os.Stderr, "usage: myapp [-v] file")
        os.Exit(1)
    }
    
    // Process file...
}
```

## Level 2: Web Basics (Days 4-7)

### Project: REST API
Simple CRUD API with in-memory storage.

Learn:
- net/http
- JSON encoding
- Routing
- Middleware

## Level 3: Concurrency (Week 2)

### Project: Concurrent File Processor
Process multiple files in parallel, aggregate results.

Learn:
- Goroutines
- Channels
- WaitGroup
- Error handling in concurrent code

## Level 4: Database Integration (Week 3)

### Project: URL Shortener
Full CRUD with PostgreSQL, rate limiting.

Learn:
- database/sql
- Connection pooling
- Transactions
- Proper error handling

## Level 5: Production Service (Month 1)

### Project: Microservice
Complete with:
- Configuration management
- Graceful shutdown
- Health checks
- Structured logging
- Metrics
- Docker deployment

## Project Ideas by Category

### CLI Tools
- File search (like `grep`)
- JSON formatter
- Log parser
- System monitor

### Web Services
- URL shortener
- Paste bin
- Link aggregator
- API gateway

### Utilities
- File synchronizer
- Backup tool
- Configuration manager

### Network
- TCP chat server
- HTTP proxy
- Port scanner
- DNS resolver

## From routermgr to Practice

Your `routermgr_grpc.go` demonstrates:
- gRPC service implementation
- Concurrent map access with mutexes
- System command execution
- Network address handling

Practice project: Build a similar service that:
1. Accepts gRPC requests
2. Manages state with proper locking
3. Executes system commands
4. Has proper error handling and logging

---

**Summary**: Start with CLI tools, progress to web APIs, add concurrency, then databases. Build complete projects rather than isolated exercises.

