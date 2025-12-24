# Topic 18: defer (RAII-like Behavior)

## 1. Problem It Solves (Engineering Motivation)

Resource cleanup is error-prone:
- Forgetting to close files
- Forgetting to unlock mutexes
- Multiple return paths miss cleanup
- Exceptions bypass cleanup code (in other languages)

```
┌─────────────────────────────────────────────────────────────────┐
│                   Resource Cleanup Problem                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C Style (error-prone):          Go with defer (robust):         │
│                                                                  │
│  int process() {                 func process() error {          │
│      FILE* f = fopen(...);           f, err := os.Open(...)      │
│      if (f == NULL) return -1;       if err != nil {             │
│                                          return err              │
│      if (error1) {                   }                           │
│          fclose(f);  // Must close   defer f.Close()  // Always! │
│          return -1;                                              │
│      }                               if error1 {                 │
│                                          return err1             │
│      if (error2) {                   }                           │
│          fclose(f);  // Must close                               │
│          return -1;                  if error2 {                 │
│      }                                   return err2             │
│                                      }                           │
│      fclose(f);      // Must close                               │
│      return 0;                       return nil                  │
│  }                               }                               │
│                                  // f.Close() called             │
│  3 places to add fclose!         // automatically                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
`defer` 语句确保函数在返回前执行指定的代码，无论函数如何返回（正常返回、提前 return、panic）。这类似于 C++ 的 RAII（资源获取即初始化），但更明确。它非常适合释放资源、解锁互斥锁、关闭文件等操作。

## 2. Core Idea and Mental Model

**defer schedules a function call to run when the surrounding function returns.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    defer Execution Order                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  func example() {                                                │
│      defer fmt.Println("first")   // Scheduled 1st               │
│      defer fmt.Println("second")  // Scheduled 2nd               │
│      defer fmt.Println("third")   // Scheduled 3rd               │
│      fmt.Println("done")                                         │
│  }                                                               │
│                                                                  │
│  Output:                                                         │
│  done       ←── Executed immediately                             │
│  third      ←── LIFO: Last In, First Out                         │
│  second                                                          │
│  first                                                           │
│                                                                  │
│  Stack visualization:                                            │
│  ┌─────────────┐                                                 │
│  │ defer third │ ◄── Top (executed first)                        │
│  ├─────────────┤                                                 │
│  │ defer second│                                                 │
│  ├─────────────┤                                                 │
│  │ defer first │ ◄── Bottom (executed last)                      │
│  └─────────────┘                                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Basic defer

```go
func readFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()  // Will run when function returns
    
    // Use file...
    return nil
}
```

### defer with Mutex

```go
var mu sync.Mutex

func criticalSection() {
    mu.Lock()
    defer mu.Unlock()  // Always unlocks, even on panic
    
    // Critical section...
}
```

### defer Argument Evaluation

```go
// Arguments evaluated when defer is called, not when it runs
func example() {
    x := 10
    defer fmt.Println("x =", x)  // Captures x=10
    x = 20
}
// Prints: x = 10

// To capture later value, use closure
func example2() {
    x := 10
    defer func() { fmt.Println("x =", x) }()  // Closure captures variable
    x = 20
}
// Prints: x = 20
```

### defer with Named Return Values

```go
// Can modify named return values
func readAll(path string) (data []byte, err error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr  // Set return value if no prior error
        }
    }()
    
    return io.ReadAll(f)
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
func StartGrpcServer() {
    lis, err := net.Listen("tcp", GrpcPort)
    if err != nil {
        log.Errorf("failed to listen: %v", err)
        return
    }
    defer lis.Close()  // Ensures listener is closed on return
    
    // ...rest of function
    grpcServer.Serve(lis)
}

// Mutex protection pattern
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // ...
    
    routeMutex.Lock()
    // ... modify shared state ...
    routeMutex.Unlock()
    
    // Better with defer:
    routeMutex.Lock()
    defer routeMutex.Unlock()
    // ... modify shared state (unlock happens automatically)
}
```

### Common Patterns

```go
// Pattern 1: File handling
func processFile(path string) error {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    defer f.Close()
    
    // Write to file...
    return nil
}

// Pattern 2: Mutex
func (c *Counter) Increment() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.value++
}

// Pattern 3: HTTP response body
func fetchURL(url string) ([]byte, error) {
    resp, err := http.Get(url)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()
    
    return io.ReadAll(resp.Body)
}

// Pattern 4: Timing
func measureTime(name string) func() {
    start := time.Now()
    return func() {
        log.Printf("%s took %v", name, time.Since(start))
    }
}

func slowOperation() {
    defer measureTime("slowOperation")()
    // ... slow code ...
}

// Pattern 5: Recovery
func safeOperation() (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
        }
    }()
    
    riskyCode()
    return nil
}
```

## 5. Common Mistakes and Pitfalls

1. **defer in loop (resource accumulation)**:
   ```go
   // WRONG: files not closed until function returns
   func processFiles(paths []string) error {
       for _, path := range paths {
           f, err := os.Open(path)
           if err != nil {
               return err
           }
           defer f.Close()  // All files open until end!
           // ...
       }
       return nil
   }
   
   // CORRECT: wrap in function
   func processFiles(paths []string) error {
       for _, path := range paths {
           if err := processFile(path); err != nil {
               return err
           }
       }
       return nil
   }
   
   func processFile(path string) error {
       f, err := os.Open(path)
       if err != nil {
           return err
       }
       defer f.Close()
       // ...
       return nil
   }
   ```

2. **defer before error check**:
   ```go
   // WRONG: nil pointer dereference if Open fails
   f, err := os.Open(path)
   defer f.Close()  // f might be nil!
   if err != nil {
       return err
   }
   
   // CORRECT: defer after error check
   f, err := os.Open(path)
   if err != nil {
       return err
   }
   defer f.Close()  // f is definitely not nil
   ```

3. **Ignoring error from Close**:
   ```go
   // May lose data if write is buffered
   defer f.Close()  // Error ignored!
   
   // Better for writes:
   defer func() {
       if err := f.Close(); err != nil {
           log.Printf("error closing file: %v", err)
       }
   }()
   
   // Or use named returns:
   func writeFile(path string, data []byte) (err error) {
       f, err := os.Create(path)
       if err != nil {
           return err
       }
       defer func() {
           if closeErr := f.Close(); err == nil {
               err = closeErr
           }
       }()
       _, err = f.Write(data)
       return err
   }
   ```

4. **Argument evaluation timing**:
   ```go
   // WRONG expectation
   i := 0
   defer fmt.Println(i)  // Prints 0, not 10
   i = 10
   
   // Use closure for current value at execution time
   i := 0
   defer func() { fmt.Println(i) }()  // Prints 10
   i = 10
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C++ RAII | Go defer |
|--------|----------|----------|
| Mechanism | Destructor | Deferred function |
| Scope | Object lifetime | Function return |
| Order | Reverse declaration | LIFO |
| Customization | In destructor | Any function |
| Explicitness | Implicit | Explicit |

### C++ RAII

```cpp
class FileGuard {
    FILE* file;
public:
    FileGuard(const char* path) : file(fopen(path, "r")) {
        if (!file) throw std::runtime_error("open failed");
    }
    ~FileGuard() {
        fclose(file);  // Called automatically at scope exit
    }
    FILE* get() { return file; }
};

void process() {
    FileGuard guard("data.txt");
    // Use guard.get()...
}  // ~FileGuard called automatically
```

### Go defer

```go
func process() error {
    f, err := os.Open("data.txt")
    if err != nil {
        return err
    }
    defer f.Close()  // Called when function returns
    
    // Use f...
    return nil
}  // f.Close() called here
```

### Linux Kernel Pattern

```c
// Kernel: goto-based cleanup
static int my_init(void) {
    int ret;
    
    ret = alloc_resource1();
    if (ret)
        goto fail1;
    
    ret = alloc_resource2();
    if (ret)
        goto fail2;
    
    return 0;
    
fail2:
    free_resource1();
fail1:
    return ret;
}

// Go equivalent is cleaner:
func myInit() error {
    r1, err := allocResource1()
    if err != nil {
        return err
    }
    defer freeResource1(r1)
    
    r2, err := allocResource2()
    if err != nil {
        return err
    }
    defer freeResource2(r2)
    
    return nil
}
```

## 7. A Small But Complete Go Example

```go
// defer_demo.go - Demonstrating defer patterns
package main

import (
    "fmt"
    "os"
    "sync"
    "time"
)

// ===== BASIC DEFER =====

func demonstrateOrder() {
    fmt.Println("=== defer Order (LIFO) ===")
    defer fmt.Println("1. First defer")
    defer fmt.Println("2. Second defer")
    defer fmt.Println("3. Third defer")
    fmt.Println("Function body")
}

// ===== RESOURCE CLEANUP =====

func createTempFile() (*os.File, error) {
    f, err := os.CreateTemp("", "demo")
    if err != nil {
        return nil, err
    }
    fmt.Printf("Created temp file: %s\n", f.Name())
    return f, nil
}

func demonstrateFileCleanup() {
    fmt.Println("\n=== File Cleanup ===")
    
    f, err := createTempFile()
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    defer func() {
        f.Close()
        os.Remove(f.Name())
        fmt.Println("File cleaned up")
    }()
    
    // Simulate work with file
    f.WriteString("Hello, defer!")
    fmt.Println("Wrote to file")
}

// ===== MUTEX PATTERN =====

type SafeCounter struct {
    mu    sync.Mutex
    value int
}

func (c *SafeCounter) Increment() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.value++
}

func (c *SafeCounter) Value() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.value
}

func demonstrateMutex() {
    fmt.Println("\n=== Mutex Pattern ===")
    counter := &SafeCounter{}
    
    var wg sync.WaitGroup
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            counter.Increment()
        }()
    }
    wg.Wait()
    
    fmt.Printf("Final counter value: %d\n", counter.Value())
}

// ===== TIMING PATTERN =====

func timeTrack(start time.Time, name string) {
    elapsed := time.Since(start)
    fmt.Printf("%s took %v\n", name, elapsed)
}

func slowFunction() {
    defer timeTrack(time.Now(), "slowFunction")
    time.Sleep(100 * time.Millisecond)
}

func demonstrateTiming() {
    fmt.Println("\n=== Timing Pattern ===")
    slowFunction()
}

// ===== NAMED RETURN VALUES =====

func divide(a, b int) (result int, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
            result = 0
        }
    }()
    
    if b == 0 {
        panic("division by zero")
    }
    return a / b, nil
}

func demonstrateNamedReturns() {
    fmt.Println("\n=== Named Returns with Recover ===")
    
    result, err := divide(10, 2)
    fmt.Printf("10/2 = %d, err = %v\n", result, err)
    
    result, err = divide(10, 0)
    fmt.Printf("10/0 = %d, err = %v\n", result, err)
}

// ===== ARGUMENT EVALUATION =====

func demonstrateArgEval() {
    fmt.Println("\n=== Argument Evaluation ===")
    
    x := 10
    defer fmt.Printf("Direct arg (captured at defer): x = %d\n", x)
    defer func() { fmt.Printf("Closure (captured at run): x = %d\n", x) }()
    x = 20
    fmt.Printf("Value at function body: x = %d\n", x)
}

// ===== LOOP GOTCHA =====

func demonstrateLoopGotcha() {
    fmt.Println("\n=== Loop Defer (Be Careful!) ===")
    
    // Wrong way - all defers wait until function end
    fmt.Println("Wrong way (accumulates resources):")
    for i := 0; i < 3; i++ {
        // In real code, this would accumulate file handles
        defer fmt.Printf("  defer %d\n", i)
    }
    
    fmt.Println("Function body complete")
    // All defers execute here
}

func main() {
    demonstrateOrder()
    demonstrateFileCleanup()
    demonstrateMutex()
    demonstrateTiming()
    demonstrateNamedReturns()
    demonstrateArgEval()
    demonstrateLoopGotcha()
    
    fmt.Println("\n=== Summary ===")
    fmt.Println("defer guarantees cleanup runs when function returns")
    fmt.Println("Use it for: files, mutexes, connections, timing")
    fmt.Println("Order: LIFO (last defer runs first)")
    fmt.Println("Args: evaluated at defer statement, not at run time")
}
```

Output:
```
=== defer Order (LIFO) ===
Function body
3. Third defer
2. Second defer
1. First defer

=== File Cleanup ===
Created temp file: /tmp/demo123456789
Wrote to file
File cleaned up

=== Mutex Pattern ===
Final counter value: 100

=== Timing Pattern ===
slowFunction took 100.123456ms

=== Named Returns with Recover ===
10/2 = 5, err = <nil>
10/0 = 0, err = panic: division by zero

=== Argument Evaluation ===
Value at function body: x = 20
Closure (captured at run): x = 20
Direct arg (captured at defer): x = 10

=== Loop Defer (Be Careful!) ===
Wrong way (accumulates resources):
Function body complete
  defer 2
  defer 1
  defer 0

=== Summary ===
defer guarantees cleanup runs when function returns
Use it for: files, mutexes, connections, timing
Order: LIFO (last defer runs first)
Args: evaluated at defer statement, not at run time
```

---

**Summary**: `defer` provides RAII-like cleanup guarantees in Go. Deferred functions run when the surrounding function returns, in LIFO order. Use `defer` for file closing, mutex unlocking, resource cleanup, and timing. Be aware that arguments are evaluated when `defer` is called, and be careful with `defer` in loops. Always place `defer` immediately after acquiring a resource and after checking for errors.

