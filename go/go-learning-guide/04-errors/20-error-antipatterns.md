# Topic 20: Common Anti-Patterns in Go Error Handling

## 1. Problem It Solves (Engineering Motivation)

Even with Go's explicit error handling, common mistakes reduce code quality:
- Errors silently ignored
- Errors logged but not handled
- Error context lost during propagation
- Inconsistent error handling patterns

```
┌─────────────────────────────────────────────────────────────────┐
│                    Error Handling Spectrum                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ◄── BAD ──────────────────────────────────────────── GOOD ──►  │
│                                                                  │
│  Ignore       Log only      Log + return    Wrap + return       │
│  ┌──────┐     ┌──────┐      ┌──────────┐    ┌────────────┐     │
│  │ f()  │     │ if err │    │ if err   │    │ if err     │     │
│  │      │     │   log  │    │   log    │    │   return   │     │
│  │      │     │        │    │   return │    │   wrap(err)│     │
│  └──────┘     └──────────┘  └──────────┘    └────────────┘     │
│                                                                  │
│  "Works"      "At least     "Stops but     "Full context        │
│  until it     we know"      no context"    for debugging"       │
│  doesn't                                                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
即使 Go 有显式的错误处理，仍有很多常见的反模式会降低代码质量。本节介绍这些反模式及其解决方案：忽略错误、只记录不处理、丢失错误上下文等问题。

## 2. Core Idea and Mental Model

**Error handling goals**:
1. Errors must be acknowledged (not ignored)
2. Errors must be propagated (not swallowed)
3. Errors must carry context (for debugging)
4. Error handling must be consistent (team patterns)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Anti-Pattern Catalog                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Silent Ignore         6. Log and Return (duplication)       │
│  2. Blank Identifier _    7. Panic for Regular Errors           │
│  3. Log Without Return    8. Error Message Stuttering           │
│  4. Naked Return          9. Not Using errors.Is/As             │
│  5. Losing Error Chain   10. Sentinel Error Export              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Anti-Patterns and Solutions

### Anti-Pattern 1: Silent Ignore

```go
// ❌ BAD: Completely ignoring error
data, _ := ioutil.ReadFile("config.json")
// Program continues with nil data

// ❌ BAD: Not checking error at all
json.Unmarshal(data, &config)

// ✅ GOOD: Handle the error
data, err := ioutil.ReadFile("config.json")
if err != nil {
    return fmt.Errorf("read config: %w", err)
}
```

### Anti-Pattern 2: Log Without Propagating

```go
// ❌ BAD: Log but continue with invalid state
func process() {
    data, err := fetchData()
    if err != nil {
        log.Printf("failed to fetch: %v", err)
        // Continues with nil data!
    }
    use(data)  // BUG: data may be nil
}

// ✅ GOOD: Log and return/handle
func process() error {
    data, err := fetchData()
    if err != nil {
        return fmt.Errorf("fetch data: %w", err)
    }
    use(data)
    return nil
}
```

### Anti-Pattern 3: Duplicate Logging

```go
// ❌ BAD: Logs at every layer
func layer1() error {
    if err := layer2(); err != nil {
        log.Printf("layer1: %v", err)  // Logs
        return err
    }
    return nil
}

func layer2() error {
    if err := layer3(); err != nil {
        log.Printf("layer2: %v", err)  // Logs again
        return err
    }
    return nil
}
// Output: layer2: error
//         layer1: error  ← Duplicated!

// ✅ GOOD: Wrap errors, log at top level only
func layer1() error {
    if err := layer2(); err != nil {
        return fmt.Errorf("layer1: %w", err)
    }
    return nil
}

func layer2() error {
    if err := layer3(); err != nil {
        return fmt.Errorf("layer2: %w", err)
    }
    return nil
}

// In main/handler:
if err := layer1(); err != nil {
    log.Printf("operation failed: %v", err)
    // Output: operation failed: layer1: layer2: original error
}
```

### Anti-Pattern 4: Losing Error Chain

```go
// ❌ BAD: Creates new error, loses original
func process(path string) error {
    _, err := os.Open(path)
    if err != nil {
        return errors.New("failed to process file")
        // Lost: was it not found? permission denied?
    }
    return nil
}

// ✅ GOOD: Wrap with context
func process(path string) error {
    _, err := os.Open(path)
    if err != nil {
        return fmt.Errorf("process %s: %w", path, err)
    }
    return nil
}

// Now caller can check:
if errors.Is(err, os.ErrNotExist) { ... }
```

### Anti-Pattern 5: Error Message Stuttering

```go
// ❌ BAD: Redundant "error" or "failed"
return fmt.Errorf("error processing file: %w", err)
// Results in: "error processing file: open file: no such file"

return fmt.Errorf("failed to read config: %w", err)
// Results in: "failed to read config: read config.json: permission denied"

// ✅ GOOD: Concise, context-focused
return fmt.Errorf("process file: %w", err)
return fmt.Errorf("read config: %w", err)
// Results in: "process file: open file: no such file"
```

### Anti-Pattern 6: Checking Error Strings

```go
// ❌ BAD: Fragile string comparison
if err.Error() == "not found" {
    // Handle...
}

if strings.Contains(err.Error(), "timeout") {
    // Handle...
}

// ✅ GOOD: Use errors.Is and errors.As
if errors.Is(err, ErrNotFound) {
    // Handle...
}

var netErr net.Error
if errors.As(err, &netErr) && netErr.Timeout() {
    // Handle timeout...
}
```

### Anti-Pattern 7: Panic for Normal Errors

```go
// ❌ BAD: Panic for expected failures
func readConfig(path string) Config {
    data, err := os.ReadFile(path)
    if err != nil {
        panic(err)  // Config missing is expected!
    }
    var cfg Config
    json.Unmarshal(data, &cfg)
    return cfg
}

// ✅ GOOD: Return error for expected failures
func readConfig(path string) (Config, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return Config{}, fmt.Errorf("read config: %w", err)
    }
    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        return Config{}, fmt.Errorf("parse config: %w", err)
    }
    return cfg, nil
}

// Panic only for truly impossible conditions (bugs)
```

### Anti-Pattern 8: Naked Returns with Errors

```go
// ❌ BAD: Named returns hide error handling
func process() (result Result, err error) {
    data, err := fetch()
    if err != nil {
        return  // What is being returned?
    }
    result, err = transform(data)
    return  // Unclear!
}

// ✅ GOOD: Explicit returns
func process() (Result, error) {
    data, err := fetch()
    if err != nil {
        return Result{}, fmt.Errorf("fetch: %w", err)
    }
    result, err := transform(data)
    if err != nil {
        return Result{}, fmt.Errorf("transform: %w", err)
    }
    return result, nil
}
```

## 4. Real-World Patterns

From `routermgr_grpc.go` - Good and improvable patterns:

```go
// Current code (acceptable but could be improved):
func CalculateCIDRBase(ip string, prefix uint32) (string, error) {
    _, network, err := net.ParseCIDR(fmt.Sprintf("%s/%d", ip, prefix))
    if err != nil {
        log.Error("failed to parse CIDR")  // Logs but also returns error
        return "", err
    }
    return network.String(), nil
}

// Improved (log at top level, wrap here):
func CalculateCIDRBase(ip string, prefix uint32) (string, error) {
    cidr := fmt.Sprintf("%s/%d", ip, prefix)
    _, network, err := net.ParseCIDR(cidr)
    if err != nil {
        return "", fmt.Errorf("parse CIDR %s: %w", cidr, err)
    }
    return network.String(), nil
}

// Current pattern (good - explicit error handling):
func (s *routermgrServer) AddRouteV4(...) (*routermgrpb.RouteActionResponse, error) {
    vrfId := route.VrfId
    if vrfId == InvalidVrfId {
        vrfId = DefaultVrfId
    }
    
    routeMutex.Lock()
    // ... critical section ...
    routeMutex.Unlock()
    
    success := MgmtdAddRouteIpv4(vrfId, route.PrefixLength, route.IpAddress, route.NextHopAddress, route.Tag)
    return &routermgrpb.RouteActionResponse{Success: success}, nil
}
```

## 5. Common Mistakes Summary

```go
// ===== COMPREHENSIVE CHECKLIST =====

// 1. Always check errors
result, err := operation()
if err != nil { /* handle */ }

// 2. Don't ignore with blank identifier unless intentional
_ = file.Close()  // Only if truly don't care

// 3. Wrap errors with context
return fmt.Errorf("operation name: %w", err)

// 4. Don't double-log
// Log at API boundary, wrap everywhere else

// 5. Use errors.Is/As for checking
if errors.Is(err, ErrNotFound) { }
var myErr *MyError
if errors.As(err, &myErr) { }

// 6. Keep error messages concise
// Good: "open file: %w"
// Bad:  "error: failed to open file: %w"

// 7. Return error, don't panic
// Panic only for bugs/impossible states

// 8. Make zero value useful for error recovery
type Result struct {
    Data   []byte
    Valid  bool
}
// Caller can check Valid even if error occurred
```

## 6. Comparison to Other Languages

| Anti-Pattern | Java | Python | Go |
|--------------|------|--------|-----|
| Silent catch | `catch (Exception e) {}` | `except: pass` | `f()` without checking |
| Over-catching | `catch (Exception e)` | `except Exception:` | Checking string content |
| Lost context | `throw new Ex(msg)` | `raise Ex(msg)` | `errors.New(msg)` |
| Log and swallow | Common | Common | Still happens |

## 7. A Small But Complete Go Example

```go
// antipatterns.go - Demonstrating error anti-patterns and fixes
package main

import (
    "errors"
    "fmt"
    "os"
)

// Sentinel errors
var (
    ErrNotFound = errors.New("not found")
    ErrInvalid  = errors.New("invalid")
)

// ===== ANTI-PATTERN EXAMPLES =====

// Anti-pattern 1: Ignoring error
func bad_ignoreError() string {
    data, _ := os.ReadFile("config.json")  // ❌ Error ignored
    return string(data)
}

// Anti-pattern 2: Log but continue
func bad_logAndContinue() {
    data, err := os.ReadFile("config.json")
    if err != nil {
        fmt.Printf("warning: %v\n", err)  // ❌ Logs but continues
    }
    _ = data  // Continues with nil data
}

// Anti-pattern 3: Losing error chain
func bad_loseChain(id string) error {
    if id == "" {
        return ErrInvalid
    }
    _, err := lookupUser(id)
    if err != nil {
        return errors.New("user lookup failed")  // ❌ Lost original error
    }
    return nil
}

// Anti-pattern 4: String comparison
func bad_stringCompare(err error) bool {
    return err != nil && err.Error() == "not found"  // ❌ Fragile
}

// ===== CORRECT PATTERNS =====

// Good: Handle or propagate
func good_handleError() (string, error) {
    data, err := os.ReadFile("config.json")
    if err != nil {
        return "", fmt.Errorf("read config: %w", err)  // ✅ Wrap and return
    }
    return string(data), nil
}

// Good: Wrap with context
func good_wrapError(id string) error {
    if id == "" {
        return fmt.Errorf("lookup user: %w", ErrInvalid)  // ✅ Context + chain
    }
    user, err := lookupUser(id)
    if err != nil {
        return fmt.Errorf("lookup user %s: %w", id, err)  // ✅ Context + chain
    }
    _ = user
    return nil
}

// Good: Use errors.Is
func good_checkError(err error) bool {
    return errors.Is(err, ErrNotFound)  // ✅ Type-safe check
}

// Helper function for examples
func lookupUser(id string) (string, error) {
    users := map[string]string{
        "1": "Alice",
        "2": "Bob",
    }
    user, ok := users[id]
    if !ok {
        return "", ErrNotFound
    }
    return user, nil
}

// ===== DEMONSTRATION =====

func main() {
    fmt.Println("===== Error Anti-Patterns Demo =====\n")
    
    // Demonstrate good error handling
    fmt.Println("1. Good error handling:")
    _, err := good_handleError()
    if err != nil {
        fmt.Printf("   Result: %v\n", err)
        fmt.Printf("   Is os.ErrNotExist: %v\n", errors.Is(err, os.ErrNotExist))
    }
    
    // Demonstrate error chain preservation
    fmt.Println("\n2. Error chain preservation:")
    err = good_wrapError("999")
    fmt.Printf("   Full error: %v\n", err)
    fmt.Printf("   Is ErrNotFound: %v\n", errors.Is(err, ErrNotFound))
    
    // Demonstrate validation error
    fmt.Println("\n3. Validation error:")
    err = good_wrapError("")
    fmt.Printf("   Full error: %v\n", err)
    fmt.Printf("   Is ErrInvalid: %v\n", errors.Is(err, ErrInvalid))
    
    // Error unwrapping
    fmt.Println("\n4. Error unwrapping:")
    err = good_wrapError("999")
    for e := err; e != nil; e = errors.Unwrap(e) {
        fmt.Printf("   → %q\n", e.Error())
    }
    
    // Best practices summary
    fmt.Println("\n===== Best Practices =====")
    fmt.Println("✓ Always check errors")
    fmt.Println("✓ Wrap with context using %w")
    fmt.Println("✓ Use errors.Is/As for checking")
    fmt.Println("✓ Log at boundaries, wrap in between")
    fmt.Println("✓ Keep messages concise")
    fmt.Println("✗ Don't ignore errors")
    fmt.Println("✗ Don't compare error strings")
    fmt.Println("✗ Don't panic for expected errors")
}
```

Output:
```
===== Error Anti-Patterns Demo =====

1. Good error handling:
   Result: read config: open config.json: no such file or directory
   Is os.ErrNotExist: true

2. Error chain preservation:
   Full error: lookup user 999: not found
   Is ErrNotFound: true

3. Validation error:
   Full error: lookup user: invalid
   Is ErrInvalid: true

4. Error unwrapping:
   → "lookup user 999: not found"
   → "not found"

===== Best Practices =====
✓ Always check errors
✓ Wrap with context using %w
✓ Use errors.Is/As for checking
✓ Log at boundaries, wrap in between
✓ Keep messages concise
✗ Don't ignore errors
✗ Don't compare error strings
✗ Don't panic for expected errors
```

---

**Summary**: Avoid common error handling anti-patterns: don't ignore errors, don't log without returning, don't lose the error chain, don't compare error strings. Instead: always check errors, wrap with context using `%w`, use `errors.Is/As` for checking, log at API boundaries only, and keep error messages concise. Good error handling makes debugging easier and code more maintainable.

