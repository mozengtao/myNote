# Topic 17: panic / recover (When to Use, When Not To)

## 1. Problem It Solves (Engineering Motivation)

Not all errors are recoverable at the call site:
- Programmer errors (nil pointer, index out of bounds)
- Invariant violations (should never happen)
- Unrecoverable situations (out of memory)

Go provides `panic` for these cases, but it should be rare.

```
┌─────────────────────────────────────────────────────────────────┐
│                   Error vs Panic Decision                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Use error when:                 Use panic when:                 │
│  ─────────────────               ─────────────────               │
│  • File not found               • Programmer bug                 │
│  • Network timeout              • Impossible state               │
│  • Invalid user input           • Initialization failure         │
│  • Resource temporarily         • Invariant violation            │
│    unavailable                  • Index out of bounds            │
│                                 • Nil pointer dereference        │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ "Don't panic" - The Hitchhiker's Guide to the Galaxy   │    │
│  │ "Don't panic" - Also good advice for Go programmers    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Rule of thumb:                                                  │
│  • Errors: Expected failures → return error                      │
│  • Panic: Bugs/impossible states → crash (fail fast)            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
`panic` 用于不可恢复的错误，如程序 bug 或不可能发生的状态。与异常不同，panic 不应该用于正常的错误处理。`recover` 允许捕获 panic，但应该只在特定场景使用（如 HTTP 中间件防止单个请求崩溃整个服务器）。

## 2. Core Idea and Mental Model

**Panic**: Immediately unwind the call stack, running deferred functions.
**Recover**: Called in a deferred function to catch a panic.

```
┌─────────────────────────────────────────────────────────────────┐
│                    Panic Execution Flow                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Normal execution:        Panic execution:                       │
│                                                                  │
│  main()                   main()                                 │
│    │                        │                                    │
│    ▼                        ▼                                    │
│  foo()                    foo()                                  │
│    │                        │                                    │
│    ▼                        ▼                                    │
│  bar()                    bar()                                  │
│    │                        │                                    │
│    ▼                        ▼                                    │
│  return                   panic("oops!")                         │
│    │                        │                                    │
│    ▼                        ▼ (unwind)                           │
│  bar() done               bar() defers run                       │
│    │                        │                                    │
│    ▼                        ▼ (unwind)                           │
│  foo() done               foo() defers run                       │
│    │                        │                                    │
│    ▼                        ▼ (unwind)                           │
│  main() done              main() defers run                      │
│                             │                                    │
│                             ▼                                    │
│                           CRASH (unless recovered)               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Panic

```go
// Built-in function
panic("something went terribly wrong")
panic(fmt.Errorf("initialization failed: %v", err))

// Implicit panics (runtime)
var p *int
*p = 42  // nil pointer dereference → panic

arr := [3]int{1, 2, 3}
_ = arr[10]  // index out of range → panic
```

### Recover

```go
// Can only be called inside a deferred function
func safeCall(fn func()) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic recovered: %v", r)
        }
    }()
    
    fn()  // May panic
    return nil
}
```

### Proper Panic/Recover Pattern

```go
// HTTP middleware to prevent one request crashing server
func RecoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("Panic recovered: %v\n%s", err, debug.Stack())
                http.Error(w, "Internal Server Error", 500)
            }
        }()
        next.ServeHTTP(w, r)
    })
}
```

## 4. Typical Real-World Usage

From your codebase context - when to panic vs error:

```go
// ERROR: Network failure (expected, recoverable)
func StartGrpcServer() {
    lis, err := net.Listen("tcp", GrpcPort)
    if err != nil {
        log.Errorf("failed to listen: %v", err)
        return  // Return error, don't panic
    }
}

// PANIC would be appropriate: initialization invariant
func MustLoadConfig(path string) *Config {
    cfg, err := LoadConfig(path)
    if err != nil {
        panic(fmt.Sprintf("failed to load config: %v", err))
    }
    return cfg
}

// Use Must prefix convention for panic-on-error functions
template.Must(template.ParseFiles("templates/*.html"))
regexp.MustCompile(`^\d{3}-\d{4}$`)
```

### When Panic is Appropriate

```go
// 1. Initialization that must succeed
func main() {
    db := MustConnectDB()  // Server can't run without DB
    defer db.Close()
    
    http.ListenAndServe(":8080", nil)
}

// 2. Unreachable code / impossible state
func processType(t Type) {
    switch t {
    case TypeA:
        handleA()
    case TypeB:
        handleB()
    default:
        panic(fmt.Sprintf("unknown type: %v", t))  // Bug if reached
    }
}

// 3. Internal library errors (recover at API boundary)
func (p *Parser) parseExpression() Expr {
    // Internal: may panic on malformed input
    // Caller wraps in recover
}
```

### Recovery Boundaries

```go
// HTTP Server: recover per request
mux := http.NewServeMux()
mux.HandleFunc("/", handler)
http.ListenAndServe(":8080", RecoveryMiddleware(mux))

// Worker pool: recover per job
for job := range jobs {
    func() {
        defer func() {
            if r := recover(); r != nil {
                log.Printf("Job %v panicked: %v", job.ID, r)
            }
        }()
        processJob(job)
    }()
}
```

## 5. Common Mistakes and Pitfalls

1. **Using panic for normal errors**:
   ```go
   // WRONG: panic for expected errors
   func readFile(path string) []byte {
       data, err := os.ReadFile(path)
       if err != nil {
           panic(err)  // NO! File not found is expected
       }
       return data
   }
   
   // CORRECT: return error
   func readFile(path string) ([]byte, error) {
       return os.ReadFile(path)
   }
   ```

2. **Recovering and continuing incorrectly**:
   ```go
   // WRONG: recovering and pretending nothing happened
   defer func() {
       recover()  // Swallows panic silently
   }()
   
   // CORRECT: log and handle appropriately
   defer func() {
       if r := recover(); r != nil {
           log.Printf("PANIC: %v\n%s", r, debug.Stack())
           // Set error return, notify monitoring, etc.
       }
   }()
   ```

3. **Panic across goroutine boundaries**:
   ```go
   // WRONG: panic in goroutine crashes program
   go func() {
       panic("oops")  // Crashes entire program!
   }()
   
   // CORRECT: recover in each goroutine
   go func() {
       defer func() {
           if r := recover(); r != nil {
               log.Printf("Goroutine panic: %v", r)
           }
       }()
       riskyOperation()
   }()
   ```

4. **Not including stack trace**:
   ```go
   // UNHELPFUL: no stack trace
   if r := recover(); r != nil {
       log.Printf("panic: %v", r)
   }
   
   // HELPFUL: include stack trace
   if r := recover(); r != nil {
       log.Printf("panic: %v\n%s", r, debug.Stack())
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C | C++ | Go |
|--------|---|-----|-----|
| Crash mechanism | abort(), assert() | throw | panic |
| Recovery | setjmp/longjmp | try/catch | defer/recover |
| Cleanup during unwind | None | Destructors | defer functions |
| Common use | Never catch | Catch many | Catch rarely |

### C: assert() / abort()

```c
// C: crash on impossible conditions
assert(ptr != NULL);  // Crashes in debug builds

if (impossible_condition) {
    abort();  // Immediate termination
}

// No cleanup during crash (unless using atexit)
```

### C++: Exceptions

```cpp
// C++: exceptions for error handling
try {
    riskyOperation();
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
// Destructors called during unwind
```

### Go: panic/recover

```go
// Go: panic for bugs, recover at boundaries
func main() {
    defer func() {
        if r := recover(); r != nil {
            log.Fatalf("Fatal: %v", r)
        }
    }()
    
    run()  // May panic on bugs
}
```

### Linux Kernel

```c
// Kernel: BUG_ON, WARN_ON, panic()
BUG_ON(ptr == NULL);  // Crashes kernel (oops)

if (should_never_happen) {
    panic("impossible state");  // Kernel panic
}

// WARN_ON for "shouldn't happen but not fatal"
WARN_ON(condition);  // Logs warning, continues
```

Go's panic is like `BUG_ON` - for impossible states, not normal errors.

## 7. A Small But Complete Go Example

```go
// panic_recover.go - Demonstrating proper panic/recover usage
package main

import (
    "fmt"
    "runtime/debug"
)

// ===== WHEN TO PANIC =====

// MustParseInt panics if parsing fails (initialization helper)
func MustParseInt(s string) int {
    var n int
    _, err := fmt.Sscanf(s, "%d", &n)
    if err != nil {
        panic(fmt.Sprintf("MustParseInt(%q): %v", s, err))
    }
    return n
}

// processKind panics on unknown kind (impossible state)
func processKind(kind string) {
    switch kind {
    case "A":
        fmt.Println("Processing A")
    case "B":
        fmt.Println("Processing B")
    default:
        panic(fmt.Sprintf("unknown kind: %q", kind))
    }
}

// ===== WHEN TO RECOVER =====

// safeCall recovers from panics and returns error
func safeCall(name string, fn func()) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("%s panicked: %v", name, r)
        }
    }()
    fn()
    return nil
}

// HTTP-style recovery middleware
func withRecovery(fn func()) {
    defer func() {
        if r := recover(); r != nil {
            fmt.Printf("RECOVERED: %v\n", r)
            fmt.Printf("Stack:\n%s\n", debug.Stack())
        }
    }()
    fn()
}

// ===== GOROUTINE SAFETY =====

// safeGoroutine runs fn in a goroutine with recovery
func safeGoroutine(fn func()) {
    go func() {
        defer func() {
            if r := recover(); r != nil {
                fmt.Printf("Goroutine panic recovered: %v\n", r)
            }
        }()
        fn()
    }()
}

func main() {
    fmt.Println("===== Panic/Recover Demo =====\n")
    
    // 1. Must functions for initialization
    fmt.Println("1. Must functions:")
    port := MustParseInt("8080")
    fmt.Printf("   Port: %d\n", port)
    
    // 2. Impossible state panic
    fmt.Println("\n2. Known kinds work:")
    processKind("A")
    processKind("B")
    
    // 3. Recovery from panic
    fmt.Println("\n3. Safe call with recovery:")
    err := safeCall("processKind", func() {
        processKind("unknown")  // Will panic
    })
    fmt.Printf("   Error: %v\n", err)
    
    // 4. Continuation after recovery
    fmt.Println("\n4. Program continues after recovery")
    
    // 5. Multiple operations with recovery
    fmt.Println("\n5. Batch processing with recovery:")
    operations := []struct {
        name string
        fn   func()
    }{
        {"op1", func() { fmt.Println("   op1: success") }},
        {"op2", func() { panic("op2 failed") }},
        {"op3", func() { fmt.Println("   op3: success") }},
    }
    
    for _, op := range operations {
        if err := safeCall(op.name, op.fn); err != nil {
            fmt.Printf("   %s: %v\n", op.name, err)
        }
    }
    
    // 6. Demonstrating stack trace
    fmt.Println("\n6. Recovery with stack trace:")
    withRecovery(func() {
        nestedPanic()
    })
    
    fmt.Println("\n===== Demo Complete =====")
}

func nestedPanic() {
    innerFunc()
}

func innerFunc() {
    panic("deep panic")
}
```

Output:
```
===== Panic/Recover Demo =====

1. Must functions:
   Port: 8080

2. Known kinds work:
Processing A
Processing B

3. Safe call with recovery:
   Error: processKind panicked: unknown kind: "unknown"

4. Program continues after recovery

5. Batch processing with recovery:
   op1: success
   op2: op2 panicked: op2 failed
   op3: success

6. Recovery with stack trace:
RECOVERED: deep panic
Stack:
goroutine 1 [running]:
runtime/debug.Stack()
    /usr/local/go/src/runtime/debug/stack.go:24 +0x5e
main.withRecovery.func1()
    /tmp/panic_recover.go:42 +0x45
panic({0x...})
    /usr/local/go/src/runtime/panic.go:... +0x...
main.innerFunc(...)
    /tmp/panic_recover.go:92 +0x27
main.nestedPanic(...)
    /tmp/panic_recover.go:88 +0x17
...

===== Demo Complete =====
```

---

**Summary**: Use `panic` only for truly unrecoverable situations: programmer errors, impossible states, and initialization failures. Use `recover` at API boundaries (HTTP handlers, worker loops) to prevent one bad request/job from crashing the entire program. Always log the stack trace when recovering. For normal errors, use Go's error values—panic is not a substitute for proper error handling.

