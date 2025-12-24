# Topic 19: Designing Error-Friendly APIs

## 1. Problem It Solves (Engineering Motivation)

API design affects how errors are handled:
- Can callers distinguish between error types?
- Is retry appropriate?
- What information is available for debugging?
- Can errors be programmatically processed?

```
┌─────────────────────────────────────────────────────────────────┐
│                   Error API Design Quality                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Bad API:                         Good API:                      │
│                                                                  │
│  func Fetch(url string) ([]byte, error)                         │
│  // Returns: "failed"             func Fetch(url string)         │
│  // Caller: ??? What failed?          ([]byte, error)            │
│  //         Retry? Give up?       // Returns:                    │
│  //         User error?           // *NetworkError (retry)       │
│                                   // *ValidationError (user err) │
│                                   // *AuthError (need login)     │
│                                                                  │
│  err := Fetch(url)                err := Fetch(url)              │
│  if err != nil {                  if err != nil {                │
│      log.Error(err) // ???            switch e := err.(type) {   │
│      return                           case *NetworkError:        │
│  }                                        retry()               │
│                                       case *AuthError:           │
│                                           relogin()             │
│                                       default:                   │
│                                           fail()                │
│                                       }                          │
│                                   }                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
好的错误 API 设计让调用者能够：区分不同类型的错误、决定是否重试、获取调试信息、程序化地处理错误。这需要使用 sentinel 错误、自定义错误类型、错误包装等技术。

## 2. Core Idea and Mental Model

**Error API design principles**:
1. **Sentinel errors**: Known error values for expected conditions
2. **Error types**: Structs for errors needing additional context
3. **Error wrapping**: Add context while preserving original error
4. **Error behavior**: Interfaces for error capabilities (IsTemporary, etc.)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Error Design Hierarchy                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Level 1: Simple string error                                    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ errors.New("something failed")                          │    │
│  │ Usage: Internal errors, one-off failures                │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Level 2: Sentinel errors                                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ var ErrNotFound = errors.New("not found")               │    │
│  │ Usage: Known conditions callers should handle           │    │
│  │ Check: errors.Is(err, ErrNotFound)                      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Level 3: Error types                                            │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ type ValidationError struct { Field, Msg string }       │    │
│  │ Usage: Errors with additional context                   │    │
│  │ Check: errors.As(err, &valErr)                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Level 4: Wrapped errors                                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ fmt.Errorf("operation X: %w", originalErr)              │    │
│  │ Usage: Adding context while preserving cause            │    │
│  │ Unwrap: errors.Unwrap(err)                              │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Sentinel Errors

```go
package mypackage

import "errors"

// Exported sentinel errors - part of API contract
var (
    ErrNotFound      = errors.New("not found")
    ErrAlreadyExists = errors.New("already exists")
    ErrInvalidInput  = errors.New("invalid input")
    ErrUnauthorized  = errors.New("unauthorized")
)

// Usage
func Find(id string) (*Item, error) {
    item, ok := store[id]
    if !ok {
        return nil, ErrNotFound
    }
    return item, nil
}

// Caller checks
item, err := Find("123")
if errors.Is(err, ErrNotFound) {
    // Handle not found specifically
}
```

### Custom Error Types

```go
// Error type with context
type ValidationError struct {
    Field   string
    Value   interface{}
    Message string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation error: %s %s (got %v)", 
        e.Field, e.Message, e.Value)
}

// Error with multiple issues
type ValidationErrors []ValidationError

func (e ValidationErrors) Error() string {
    var msgs []string
    for _, err := range e {
        msgs = append(msgs, err.Error())
    }
    return strings.Join(msgs, "; ")
}

// Caller extracts details
var valErr *ValidationError
if errors.As(err, &valErr) {
    fmt.Printf("Invalid field: %s\n", valErr.Field)
}
```

### Behavioral Interfaces

```go
// Error behavior interfaces
type temporary interface {
    Temporary() bool
}

type timeout interface {
    Timeout() bool
}

// Check behavior
func IsTemporary(err error) bool {
    var t temporary
    return errors.As(err, &t) && t.Temporary()
}

// Implementation
type NetworkError struct {
    Op       string
    Addr     string
    Err      error
    IsTemp   bool
}

func (e *NetworkError) Error() string {
    return fmt.Sprintf("%s %s: %v", e.Op, e.Addr, e.Err)
}

func (e *NetworkError) Temporary() bool {
    return e.IsTemp
}

func (e *NetworkError) Unwrap() error {
    return e.Err
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go` context - designing gRPC error responses:

```go
// gRPC uses status codes - map domain errors to gRPC status
import (
    "google.golang.org/grpc/codes"
    "google.golang.org/grpc/status"
)

func (s *routermgrServer) AddRouteV4(ctx context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // Validate input
    if route.IpAddress == "" {
        return nil, status.Error(codes.InvalidArgument, "IP address required")
    }
    
    // Check authorization
    if !isAuthorized(ctx) {
        return nil, status.Error(codes.PermissionDenied, "not authorized")
    }
    
    // Business logic error
    if err := addRoute(route); err != nil {
        if errors.Is(err, ErrRouteExists) {
            return nil, status.Error(codes.AlreadyExists, err.Error())
        }
        return nil, status.Errorf(codes.Internal, "add route: %v", err)
    }
    
    return &routermgrpb.RouteActionResponse{Success: true}, nil
}
```

### REST API Error Design

```go
// API error response
type APIError struct {
    Code    string `json:"code"`
    Message string `json:"message"`
    Details any    `json:"details,omitempty"`
}

// Error implementation
func (e *APIError) Error() string {
    return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

// Constructor helpers
func NewNotFoundError(resource, id string) *APIError {
    return &APIError{
        Code:    "NOT_FOUND",
        Message: fmt.Sprintf("%s with id %s not found", resource, id),
    }
}

func NewValidationError(field, issue string) *APIError {
    return &APIError{
        Code:    "VALIDATION_ERROR",
        Message: fmt.Sprintf("invalid %s: %s", field, issue),
    }
}

// Handler usage
func handleGetUser(w http.ResponseWriter, r *http.Request) {
    user, err := getUser(r.Context(), userID)
    if err != nil {
        var apiErr *APIError
        if errors.As(err, &apiErr) {
            writeJSON(w, apiErr.HTTPStatus(), apiErr)
            return
        }
        writeJSON(w, 500, &APIError{Code: "INTERNAL", Message: "internal error"})
        return
    }
    writeJSON(w, 200, user)
}
```

## 5. Common Mistakes and Pitfalls

1. **Breaking error chain with new errors**:
   ```go
   // WRONG: loses original error
   if err != nil {
       return errors.New("operation failed")
   }
   
   // CORRECT: wrap to preserve chain
   if err != nil {
       return fmt.Errorf("operation failed: %w", err)
   }
   ```

2. **Exposing internal errors to users**:
   ```go
   // WRONG: exposes SQL details
   if err != nil {
       return fmt.Errorf("database error: %w", err)
       // "database error: pq: relation "users" does not exist"
   }
   
   // CORRECT: log internal, return user-friendly
   if err != nil {
       log.Errorf("database error: %v", err)
       return ErrInternal  // Generic for user
   }
   ```

3. **Comparing errors by string**:
   ```go
   // WRONG: fragile
   if err.Error() == "not found" {
       // Handle...
   }
   
   // CORRECT: use sentinel or type
   if errors.Is(err, ErrNotFound) {
       // Handle...
   }
   ```

4. **Creating error types for every error**:
   ```go
   // OVER-ENGINEERED
   type FileOpenError struct{ ... }
   type FileReadError struct{ ... }
   type FileWriteError struct{ ... }
   type FileCloseError struct{ ... }
   
   // SIMPLER: use wrapping
   if err != nil {
       return fmt.Errorf("open %s: %w", path, err)
   }
   
   // Callers use errors.Is(err, os.ErrNotExist)
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C (errno) | C++ (exceptions) | Go |
|--------|-----------|------------------|-----|
| Error types | Integer codes | Exception classes | error interface |
| Grouping | errno.h ranges | Exception hierarchy | Sentinel + types |
| Context | errno only | Exception message | Wrapped errors |
| Checking | if (ret < 0) | try/catch | if err != nil |

### C errno

```c
// C: integer error codes
if (read(fd, buf, size) < 0) {
    switch (errno) {
        case ENOENT:
            printf("File not found\n");
            break;
        case EACCES:
            printf("Permission denied\n");
            break;
        default:
            printf("Error: %s\n", strerror(errno));
    }
}
```

### Go Equivalent

```go
// Go: error types and sentinels
data, err := os.ReadFile(path)
if err != nil {
    if errors.Is(err, os.ErrNotExist) {
        fmt.Println("File not found")
    } else if errors.Is(err, os.ErrPermission) {
        fmt.Println("Permission denied")
    } else {
        fmt.Printf("Error: %v\n", err)
    }
}
```

## 7. A Small But Complete Go Example

```go
// error_api_design.go - Demonstrating error-friendly API design
package main

import (
    "errors"
    "fmt"
)

// ===== LEVEL 1: SENTINEL ERRORS =====

var (
    ErrNotFound     = errors.New("user not found")
    ErrUnauthorized = errors.New("unauthorized")
    ErrRateLimited  = errors.New("rate limited")
)

// ===== LEVEL 2: CUSTOM ERROR TYPE =====

type ValidationError struct {
    Field   string
    Value   any
    Message string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation failed: field %q %s (got %v)",
        e.Field, e.Message, e.Value)
}

// ===== LEVEL 3: ERROR WITH BEHAVIOR =====

type RetryableError struct {
    Err       error
    RetryAfter int // seconds
}

func (e *RetryableError) Error() string {
    return fmt.Sprintf("%v (retry after %ds)", e.Err, e.RetryAfter)
}

func (e *RetryableError) Unwrap() error {
    return e.Err
}

func (e *RetryableError) Temporary() bool {
    return true
}

// ===== API IMPLEMENTATION =====

type User struct {
    ID    string
    Name  string
    Email string
}

var users = map[string]*User{
    "1": {ID: "1", Name: "Alice", Email: "alice@example.com"},
}

func GetUser(id string) (*User, error) {
    // Validation
    if id == "" {
        return nil, &ValidationError{
            Field:   "id",
            Value:   id,
            Message: "cannot be empty",
        }
    }
    
    // Auth check (simulated)
    if id == "secret" {
        return nil, ErrUnauthorized
    }
    
    // Rate limit (simulated)
    if id == "slow" {
        return nil, &RetryableError{
            Err:        ErrRateLimited,
            RetryAfter: 30,
        }
    }
    
    // Lookup
    user, ok := users[id]
    if !ok {
        return nil, fmt.Errorf("get user %s: %w", id, ErrNotFound)
    }
    
    return user, nil
}

// ===== ERROR HELPER FUNCTIONS =====

func IsTemporary(err error) bool {
    type temporary interface {
        Temporary() bool
    }
    var t temporary
    return errors.As(err, &t) && t.Temporary()
}

func GetRetryAfter(err error) (int, bool) {
    var re *RetryableError
    if errors.As(err, &re) {
        return re.RetryAfter, true
    }
    return 0, false
}

// ===== CALLER CODE =====

func main() {
    fmt.Println("===== Error API Design Demo =====\n")
    
    testCases := []string{"1", "999", "", "secret", "slow"}
    
    for _, id := range testCases {
        fmt.Printf("GetUser(%q):\n", id)
        user, err := GetUser(id)
        
        if err == nil {
            fmt.Printf("  ✓ Success: %+v\n", user)
            continue
        }
        
        // Check for specific error types
        var valErr *ValidationError
        if errors.As(err, &valErr) {
            fmt.Printf("  ✗ Validation Error: %s\n", valErr)
            fmt.Printf("    Field: %s, Value: %v\n", valErr.Field, valErr.Value)
            continue
        }
        
        // Check for specific sentinel errors
        if errors.Is(err, ErrNotFound) {
            fmt.Printf("  ✗ Not Found: %v\n", err)
            continue
        }
        
        if errors.Is(err, ErrUnauthorized) {
            fmt.Printf("  ✗ Unauthorized: %v\n", err)
            continue
        }
        
        // Check for retryable errors
        if IsTemporary(err) {
            if retryAfter, ok := GetRetryAfter(err); ok {
                fmt.Printf("  ✗ Temporary Error: %v\n", err)
                fmt.Printf("    Retry after: %d seconds\n", retryAfter)
                continue
            }
        }
        
        // Unknown error
        fmt.Printf("  ✗ Unknown Error: %v\n", err)
    }
    
    // Demonstrate error chain
    fmt.Println("\n===== Error Chain Demo =====")
    _, err := GetUser("999")
    
    fmt.Println("Error chain:")
    for e := err; e != nil; e = errors.Unwrap(e) {
        fmt.Printf("  → %v\n", e)
    }
    
    fmt.Printf("\nerrors.Is(err, ErrNotFound): %v\n", errors.Is(err, ErrNotFound))
}
```

Output:
```
===== Error API Design Demo =====

GetUser("1"):
  ✓ Success: &{ID:1 Name:Alice Email:alice@example.com}
GetUser("999"):
  ✗ Not Found: get user 999: user not found
GetUser(""):
  ✗ Validation Error: validation failed: field "id" cannot be empty (got )
    Field: id, Value: 
GetUser("secret"):
  ✗ Unauthorized: unauthorized
GetUser("slow"):
  ✗ Temporary Error: rate limited (retry after 30s)
    Retry after: 30 seconds

===== Error Chain Demo =====
Error chain:
  → get user 999: user not found
  → user not found

errors.Is(err, ErrNotFound): true
```

---

**Summary**: Good error API design enables callers to handle errors appropriately. Use sentinel errors for known conditions, custom error types for errors needing context, and error wrapping to preserve the error chain while adding context. Implement behavioral interfaces (Temporary, Timeout) when callers need to make decisions based on error properties. Don't expose internal details to users, and use `errors.Is` and `errors.As` for type-safe error checking.

