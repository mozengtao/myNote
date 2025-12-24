# Topic 16: Error Values and Explicit Error Handling

## 1. Problem It Solves (Engineering Motivation)

Error handling approaches in other languages:
- **C**: Return codes (easy to ignore, no type safety)
- **C++/Java/Python**: Exceptions (hidden control flow, expensive)
- **Rust**: Result<T, E> (similar to Go, but enforced by compiler)

Problems with exceptions:
- Non-local control flow (hard to follow)
- Easy to forget to catch
- Performance overhead
- Error information often lost in stack traces

```
┌─────────────────────────────────────────────────────────────────┐
│                 Exception vs Error Value Control Flow            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Exceptions (Java/Python):       Error Values (Go):              │
│                                                                  │
│  try {                           result, err := doWork()         │
│      result = doWork()           if err != nil {                 │
│      process(result)                 return fmt.Errorf(          │
│  } catch (IOException e) {               "work failed: %w", err) │
│      // Handle...                }                               │
│  } catch (ParseException e) {    processed, err := process(result)│
│      // Handle...                if err != nil {                 │
│  }                                   return err                  │
│                                  }                               │
│  Problems:                                                       │
│  • doWork() might throw          Advantages:                     │
│    other exceptions              • Error handling is visible     │
│  • Control flow jumps            • Can't forget to check         │
│  • What exceptions to catch?     • Local control flow            │
│                                  • Errors are just values        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 不使用异常，而是将错误作为普通返回值。函数返回 `(result, error)`，调用者必须显式检查错误。这使错误处理可见且明确，避免了异常带来的隐式控制流问题。

## 2. Core Idea and Mental Model

**In Go, errors are values, not exceptions.**

```go
// The error interface
type error interface {
    Error() string
}

// Functions return errors as the last return value
func doSomething() (Result, error)

// Caller must handle the error
result, err := doSomething()
if err != nil {
    // Handle error
}
```

```
┌─────────────────────────────────────────────────────────────────┐
│                    Error Mental Model                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  The error interface:                                            │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ type error interface {                                  │    │
│  │     Error() string                                      │    │
│  │ }                                                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Error flow:                                                     │
│                                                                  │
│  ┌────────────┐    (result, nil)    ┌────────────┐              │
│  │  Caller    │ ◄────────────────── │  Function  │              │
│  │            │    (nil, error)     │            │              │
│  └────────────┘ ◄────────────────── └────────────┘              │
│        │                                                         │
│        ▼                                                         │
│  if err != nil {                                                 │
│      // Handle: log, wrap, return, retry, fallback              │
│  }                                                               │
│  // Continue with result                                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Creating Errors

```go
import (
    "errors"
    "fmt"
)

// Simple error
err := errors.New("something went wrong")

// Formatted error
err := fmt.Errorf("failed to process %s: %v", filename, reason)

// Error wrapping (Go 1.13+)
err := fmt.Errorf("database query failed: %w", originalErr)
```

### Custom Error Types

```go
// Struct implementing error interface
type ValidationError struct {
    Field   string
    Message string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation failed for %s: %s", e.Field, e.Message)
}

// Usage
func validate(input string) error {
    if input == "" {
        return &ValidationError{Field: "input", Message: "cannot be empty"}
    }
    return nil
}
```

### Error Checking and Handling

```go
// Basic pattern
result, err := doSomething()
if err != nil {
    return err  // Propagate
}

// With context
result, err := doSomething()
if err != nil {
    return fmt.Errorf("doSomething failed: %w", err)
}

// Error type checking (Go 1.13+)
var validErr *ValidationError
if errors.As(err, &validErr) {
    fmt.Printf("Validation error on field: %s\n", validErr.Field)
}

// Error value checking
if errors.Is(err, os.ErrNotExist) {
    fmt.Println("File does not exist")
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
func CalculateCIDRBase(ip string, prefix uint32) (string, error) {
    _, network, err := net.ParseCIDR(fmt.Sprintf("%s/%d", ip, prefix))
    if err != nil {
        log.Error("failed to parse CIDR")
        return "", err  // Return error to caller
    }
    return network.String(), nil  // Success: return result and nil error
}

func StartGrpcServer() {
    lis, err := net.Listen("tcp", GrpcPort)
    if err != nil {
        log.Errorf("failed to listen: %v", err)
        return  // Handle by returning early
    }
    defer lis.Close()
    // Continue with success path...
}

// gRPC methods return error as second value
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // ... processing ...
    return &routermgrpb.RouteActionResponse{Success: success}, nil
}
```

### Error Wrapping Chain

```go
// Layer 1: Database
func queryUser(id int) (*User, error) {
    row := db.QueryRow("SELECT * FROM users WHERE id = ?", id)
    var user User
    if err := row.Scan(&user.ID, &user.Name); err != nil {
        return nil, fmt.Errorf("query user %d: %w", id, err)
    }
    return &user, nil
}

// Layer 2: Service
func getUser(id int) (*User, error) {
    user, err := queryUser(id)
    if err != nil {
        return nil, fmt.Errorf("get user: %w", err)
    }
    return user, nil
}

// Layer 3: Handler
func handleGetUser(w http.ResponseWriter, r *http.Request) {
    user, err := getUser(42)
    if err != nil {
        // Full chain: "get user: query user 42: sql: no rows in result set"
        log.Errorf("handler: %v", err)
        http.Error(w, "user not found", http.StatusNotFound)
        return
    }
    json.NewEncoder(w).Encode(user)
}
```

## 5. Common Mistakes and Pitfalls

1. **Ignoring errors**:
   ```go
   // WRONG: silently ignoring error
   result, _ := doSomething()
   
   // WRONG: not checking error
   result, err := doSomething()
   use(result)  // Bug if err != nil
   
   // CORRECT
   result, err := doSomething()
   if err != nil {
       return fmt.Errorf("do something: %w", err)
   }
   use(result)
   ```

2. **Checking error incorrectly**:
   ```go
   // WRONG: comparing error strings
   if err.Error() == "not found" { }
   
   // CORRECT: use errors.Is or errors.As
   if errors.Is(err, ErrNotFound) { }
   
   var notFoundErr *NotFoundError
   if errors.As(err, &notFoundErr) { }
   ```

3. **Returning nil error with nil result**:
   ```go
   // WRONG: ambiguous return
   func find(id int) (*User, error) {
       user := cache[id]
       return user, nil  // user might be nil!
   }
   
   // CORRECT: explicit error for not found
   func find(id int) (*User, error) {
       user, ok := cache[id]
       if !ok {
           return nil, ErrNotFound
       }
       return user, nil
   }
   ```

4. **Not wrapping errors with context**:
   ```go
   // UNHELPFUL error message
   func processFile(path string) error {
       data, err := os.ReadFile(path)
       if err != nil {
           return err  // Just "open /foo: no such file"
       }
       // ...
   }
   
   // HELPFUL: adds context
   func processFile(path string) error {
       data, err := os.ReadFile(path)
       if err != nil {
           return fmt.Errorf("process file %s: %w", path, err)
       }
       // ...
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C | C++ | Go |
|--------|---|-----|-----|
| Error signaling | Return code | Exception | Error value |
| Type safety | None (int) | Yes | Yes (error interface) |
| Ignorable | Yes | Yes (might not catch) | Yes (but obvious) |
| Control flow | Linear | Non-local | Linear |
| Performance | No overhead | Overhead on throw | No overhead |

### C Return Codes

```c
// C: return codes (easy to ignore)
int fd = open("file.txt", O_RDONLY);
if (fd < 0) {
    perror("open failed");
    return -1;
}

// Problem: nothing forces you to check
int result = some_function();
use_result(result);  // Bug if result indicates error
```

### C++ Exceptions

```cpp
// C++: exceptions
try {
    File file("data.txt");
    file.read(buffer, size);
} catch (const FileException& e) {
    std::cerr << "File error: " << e.what() << std::endl;
}

// Problem: what if read() throws something else?
```

### Go Error Values

```go
// Go: explicit, checked at call site
file, err := os.Open("data.txt")
if err != nil {
    return fmt.Errorf("open data.txt: %w", err)
}
defer file.Close()

_, err = file.Read(buffer)
if err != nil {
    return fmt.Errorf("read data.txt: %w", err)
}
```

### Linux Kernel Style

```c
// Kernel: negative errno, explicit checks
static int my_driver_read(struct file *filp, char __user *buf, 
                          size_t count, loff_t *f_pos) {
    int ret;
    
    ret = copy_to_user(buf, data, count);
    if (ret) {
        return -EFAULT;
    }
    
    return count;
}
```

Go is closest to this style: explicit returns, explicit checks, linear control flow.

## 7. A Small But Complete Go Example

```go
// errors_demo.go - Demonstrating Go error handling
package main

import (
    "errors"
    "fmt"
    "os"
)

// Sentinel errors for known conditions
var (
    ErrNotFound   = errors.New("not found")
    ErrPermission = errors.New("permission denied")
)

// Custom error type
type ValidationError struct {
    Field string
    Issue string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation failed: %s %s", e.Field, e.Issue)
}

// Simulated user database
var users = map[int]string{
    1: "alice",
    2: "bob",
}

// Layer 1: Data access
func fetchUser(id int) (string, error) {
    if id < 0 {
        return "", &ValidationError{Field: "id", Issue: "must be positive"}
    }
    
    name, ok := users[id]
    if !ok {
        return "", fmt.Errorf("user %d: %w", id, ErrNotFound)
    }
    
    return name, nil
}

// Layer 2: Business logic
func getUserGreeting(id int) (string, error) {
    name, err := fetchUser(id)
    if err != nil {
        return "", fmt.Errorf("get user greeting: %w", err)
    }
    
    return fmt.Sprintf("Hello, %s!", name), nil
}

// Layer 3: Handler
func handleRequest(id int) {
    greeting, err := getUserGreeting(id)
    
    if err != nil {
        // Check for specific error types
        var valErr *ValidationError
        if errors.As(err, &valErr) {
            fmt.Printf("Bad request: %s\n", valErr)
            return
        }
        
        // Check for specific error values
        if errors.Is(err, ErrNotFound) {
            fmt.Printf("Not found: %v\n", err)
            return
        }
        
        // Unknown error
        fmt.Printf("Internal error: %v\n", err)
        return
    }
    
    fmt.Printf("Success: %s\n", greeting)
}

func main() {
    fmt.Println("=== Error Handling Demo ===\n")
    
    fmt.Println("1. Successful request:")
    handleRequest(1)
    
    fmt.Println("\n2. Not found error:")
    handleRequest(999)
    
    fmt.Println("\n3. Validation error:")
    handleRequest(-1)
    
    fmt.Println("\n=== Error Wrapping ===\n")
    
    _, err := getUserGreeting(999)
    fmt.Printf("Full error chain: %v\n", err)
    
    // Unwrap to get original error
    fmt.Printf("Is ErrNotFound: %v\n", errors.Is(err, ErrNotFound))
    
    // Error chain in detail
    fmt.Println("\nError unwrapping:")
    for err != nil {
        fmt.Printf("  → %v\n", err)
        err = errors.Unwrap(err)
    }
    
    fmt.Println("\n=== File Error Example ===\n")
    
    _, err = os.Open("/nonexistent/file.txt")
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        
        if errors.Is(err, os.ErrNotExist) {
            fmt.Println("File does not exist (checked with errors.Is)")
        }
        
        // Access underlying PathError
        var pathErr *os.PathError
        if errors.As(err, &pathErr) {
            fmt.Printf("Operation: %s, Path: %s\n", pathErr.Op, pathErr.Path)
        }
    }
}
```

Output:
```
=== Error Handling Demo ===

1. Successful request:
Success: Hello, alice!

2. Not found error:
Not found: get user greeting: user 999: not found

3. Validation error:
Bad request: validation failed: id must be positive

=== Error Wrapping ===

Full error chain: get user greeting: user 999: not found
Is ErrNotFound: true

Error unwrapping:
  → get user greeting: user 999: not found
  → user 999: not found
  → not found

=== File Error Example ===

Error: open /nonexistent/file.txt: no such file or directory
File does not exist (checked with errors.Is)
Operation: open, Path: /nonexistent/file.txt
```

---

**Summary**: Go treats errors as values returned from functions, not as exceptions thrown. This makes error handling explicit, visible, and part of the normal control flow. Use `fmt.Errorf` with `%w` to wrap errors with context, `errors.Is` to check for specific error values, and `errors.As` to check for specific error types. Always check errors immediately after the function call.

