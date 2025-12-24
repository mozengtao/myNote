# Topic 5: Basic Types and Zero Values

## 1. Problem It Solves (Engineering Motivation)

Uninitialized variables cause:
- **C/C++**: Undefined behavior, security vulnerabilities, crashes
- **Java**: `NullPointerException` at runtime
- **C**: Garbage values, buffer overflows from uninitialized memory

The problem: What should a variable contain before explicit initialization?

Go's answer: **Every type has a defined zero value. No uninitialized memory. Ever.**

```
┌─────────────────────────────────────────────────────────────────┐
│                 Uninitialized Variables                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C/C++:                                                          │
│  ┌─────────────────────────────────────────┐                    │
│  │  int x;        // Garbage: 0xDEADBEEF   │                    │
│  │  char* s;      // Garbage: 0x????????   │                    │
│  │  if (x > 0) {  // Undefined behavior!   │                    │
│  └─────────────────────────────────────────┘                    │
│                                                                  │
│  Go:                                                             │
│  ┌─────────────────────────────────────────┐                    │
│  │  var x int      // Always 0             │                    │
│  │  var s string   // Always ""            │                    │
│  │  var p *int     // Always nil           │                    │
│  │  if x > 0 {     // Well-defined: false  │                    │
│  └─────────────────────────────────────────┘                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
在 C/C++ 中，未初始化的变量包含随机垃圾值，这是许多安全漏洞的根源。Go 保证每个变量都有一个定义的"零值"。数字是 0，字符串是空字符串，指针是 nil。这消除了一整类 bug。

## 2. Core Idea and Mental Model

**Zero value principle**: Every type has a useful default state.

Design goal: Newly declared variables should be immediately usable without explicit initialization.

```
┌─────────────────────────────────────────────────────────────────┐
│                    Zero Values by Type                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Numeric Types:                                                  │
│  ├── int, int8, int16, int32, int64       → 0                   │
│  ├── uint, uint8, uint16, uint32, uint64  → 0                   │
│  ├── float32, float64                     → 0.0                 │
│  └── complex64, complex128                → (0+0i)              │
│                                                                  │
│  Boolean:                                                        │
│  └── bool                                 → false               │
│                                                                  │
│  String:                                                         │
│  └── string                               → ""                  │
│                                                                  │
│  Pointer Types:                                                  │
│  └── *T, unsafe.Pointer                   → nil                 │
│                                                                  │
│  Reference Types:                                                │
│  ├── slice                                → nil                 │
│  ├── map                                  → nil                 │
│  ├── channel                              → nil                 │
│  ├── function                             → nil                 │
│  └── interface                            → nil                 │
│                                                                  │
│  Composite Types:                                                │
│  ├── array                                → all elements zero   │
│  └── struct                               → all fields zero     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Basic Types

```go
// Integers (platform-dependent size)
var i int       // 32 or 64 bits depending on platform
var u uint      // unsigned

// Fixed-size integers
var i8 int8     // -128 to 127
var i16 int16   // -32768 to 32767
var i32 int32   // -2^31 to 2^31-1
var i64 int64   // -2^63 to 2^63-1

// Unsigned variants
var u8 uint8    // 0 to 255 (alias: byte)
var u16 uint16  // 0 to 65535
var u32 uint32  // 0 to 2^32-1
var u64 uint64  // 0 to 2^64-1

// Floating point
var f32 float32 // IEEE-754 32-bit
var f64 float64 // IEEE-754 64-bit (default for literals)

// Complex
var c64 complex64   // two float32
var c128 complex128 // two float64

// Boolean
var b bool      // true or false

// String (immutable byte sequence)
var s string    // UTF-8 encoded

// Byte and Rune (aliases)
var by byte     // alias for uint8
var r rune      // alias for int32 (Unicode code point)
```

### Zero Value Declarations

```go
// All these are valid and have defined values
var count int       // 0
var name string     // ""
var active bool     // false
var ratio float64   // 0.0
var data []byte     // nil (but len() returns 0)
var lookup map[string]int  // nil
```

### Short Declaration

```go
// Type inferred from value
count := 42           // int
name := "Alice"       // string
ratio := 3.14         // float64
active := true        // bool

// Multiple assignment
x, y := 10, 20
a, b, c := 1, "two", 3.0
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
// Constants with typed values
const (
    GrpcPort       = ":50151"    // string
    InvalidVrfId   = 0           // int (untyped constant)
    DefaultVrfId   = 1           // int
    Ipv4Idx        = 0           // int
    Ipv6Idx        = 1           // int
    Ipv4HostPrefix = 32          // int
    Ipv6HostPrefix = 128         // int
)

// Struct with zero-value friendly design
type RouterAddress struct {
    VrfId     uint32   // Zero: 0
    Address   string   // Zero: ""
    PrefixLen uint32   // Zero: 0
    IsV6      bool     // Zero: false
}

// Zero value check in real code
func UpdateDummyAddressForVmc(vrfId uint32, isV6 bool, routerAddress RouterAddress, vmcAddress VmcAddress, isAdd bool) bool {
    // Zero value ("") means invalid/unset
    if routerAddress.Address == "" {
        return true
    }
    if vmcAddress.Address == "" {
        return true
    }
    // ...
}
```

### Zero Value Usability Pattern

```go
// sync.Mutex is usable at zero value
var mu sync.Mutex    // Ready to use, no initialization needed
mu.Lock()
mu.Unlock()

// bytes.Buffer is usable at zero value
var buf bytes.Buffer // Ready to use
buf.WriteString("hello")
fmt.Println(buf.String())

// Map is NOT usable at zero value (nil map)
var m map[string]int
m["key"] = 1  // PANIC: assignment to nil map

// Must initialize
m = make(map[string]int)
m["key"] = 1  // OK
```

## 5. Common Mistakes and Pitfalls

1. **Using nil maps**:
   ```go
   // Wrong: nil map can be read but not written
   var m map[string]int
   v := m["key"]    // OK, returns zero value (0)
   m["key"] = 1     // PANIC!
   
   // Correct: always initialize maps
   m := make(map[string]int)
   m["key"] = 1     // OK
   ```

2. **Nil slice vs empty slice confusion**:
   ```go
   var s1 []int         // nil slice
   s2 := []int{}        // empty slice (non-nil)
   s3 := make([]int, 0) // empty slice (non-nil)
   
   // All behave the same for most operations
   len(s1) == len(s2) == len(s3) == 0  // true
   
   // But different in JSON
   json.Marshal(s1) // null
   json.Marshal(s2) // []
   ```

3. **Assuming zero means unset**:
   ```go
   // Problem: 0 could be a valid value
   type Config struct {
       Port int
   }
   
   cfg := Config{}
   // Is Port 0 because unset, or intentionally 0?
   
   // Solution: use pointer for optional values
   type Config struct {
       Port *int  // nil = unset, *Port = 0 is valid
   }
   ```

4. **Not leveraging zero values in design**:
   ```go
   // Anti-pattern: constructor required
   type Counter struct {
       mu    sync.Mutex
       value int
   }
   func NewCounter() *Counter {
       return &Counter{value: 0}  // Unnecessary!
   }
   
   // Better: zero value works
   type Counter struct {
       mu    sync.Mutex
       value int
   }
   // Just use: var c Counter
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C/C++ | Go |
|--------|-------|-----|
| Uninitialized local | Garbage value | Zero value |
| Static variables | Zero-initialized | Zero value |
| Heap allocation | malloc: garbage, calloc: zero | Zero value |
| Null pointer | NULL (0) | nil |
| Empty string | "" with null terminator | "" (length 0) |

### C Memory Initialization

```c
// C: Must explicitly initialize
int arr[100];           // Garbage values
memset(arr, 0, sizeof(arr));  // Now zero

struct Foo foo;         // Garbage values
memset(&foo, 0, sizeof(foo)); // Now zero

char* s;                // Garbage pointer
s = NULL;               // Now safe
```

### Go Memory Initialization

```go
// Go: Always initialized
var arr [100]int        // All zeros
var foo Foo             // All fields zero
var s *string           // nil
```

### Linux Kernel Pattern

```c
// Kernel: often uses designated initializers
struct device dev = {
    .name = "mydev",
    // Other fields implicitly zero
};

// Go equivalent
dev := Device{
    Name: "mydev",
    // Other fields implicitly zero value
}
```

## 7. A Small But Complete Go Example

```go
// zero_values.go - Demonstrating zero value behavior
package main

import (
    "encoding/json"
    "fmt"
    "sync"
)

// User demonstrates zero-value friendly design
type User struct {
    ID       int64
    Name     string
    Email    string
    Active   bool
    mu       sync.Mutex // Unexported, zero-value ready
    loginCount int
}

// IncrementLogin is safe to call on zero-value User
func (u *User) IncrementLogin() {
    u.mu.Lock()
    defer u.mu.Unlock()
    u.loginCount++
}

// LoginCount returns the login count
func (u *User) LoginCount() int {
    u.mu.Lock()
    defer u.mu.Unlock()
    return u.loginCount
}

func main() {
    // Zero value struct is usable
    var user User
    fmt.Printf("Zero User: ID=%d, Name=%q, Active=%v\n",
        user.ID, user.Name, user.Active)
    
    // Methods work on zero value
    user.IncrementLogin()
    user.IncrementLogin()
    fmt.Printf("Login count: %d\n", user.LoginCount())
    
    // Zero values in JSON
    type Response struct {
        Items  []string       // will be null
        Items2 []string       // will be []
        Count  int            // will be 0
        Meta   map[string]any // will be null
    }
    
    resp := Response{
        Items2: []string{}, // Explicitly empty
    }
    
    data, _ := json.MarshalIndent(resp, "", "  ")
    fmt.Printf("\nJSON output:\n%s\n", data)
    
    // Demonstrating nil vs empty slice
    var nilSlice []int
    emptySlice := []int{}
    
    fmt.Printf("\nnilSlice == nil: %v\n", nilSlice == nil)
    fmt.Printf("emptySlice == nil: %v\n", emptySlice == nil)
    fmt.Printf("len(nilSlice): %d\n", len(nilSlice))
    fmt.Printf("len(emptySlice): %d\n", len(emptySlice))
    
    // Both can be appended to
    nilSlice = append(nilSlice, 1)
    emptySlice = append(emptySlice, 1)
    fmt.Printf("After append - nilSlice: %v, emptySlice: %v\n",
        nilSlice, emptySlice)
}
```

Output:
```
Zero User: ID=0, Name="", Active=false
Login count: 2

JSON output:
{
  "Items": null,
  "Items2": [],
  "Count": 0,
  "Meta": null
}

nilSlice == nil: true
emptySlice == nil: false
len(nilSlice): 0
len(emptySlice): 0
After append - nilSlice: [1], emptySlice: [1]
```

---

**Summary**: Go's zero value guarantee eliminates uninitialized memory bugs entirely. Every variable has a well-defined initial state. Good Go types are designed to be usable at their zero value (like `sync.Mutex`, `bytes.Buffer`). This is a fundamental safety improvement over C/C++ and reduces the need for constructors and initialization boilerplate.

