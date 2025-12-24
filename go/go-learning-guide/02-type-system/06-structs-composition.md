# Topic 6: Structs and Composition Over Inheritance

## 1. Problem It Solves (Engineering Motivation)

Object-oriented inheritance problems:
- **Fragile base class problem**: Changes to parent classes break children
- **Diamond problem**: Multiple inheritance ambiguity (C++)
- **Deep hierarchies**: Hard to understand, hard to modify
- **Forced abstraction**: Fitting problems into class hierarchies
- **Coupling**: Subclasses tightly coupled to parent implementation

```
┌─────────────────────────────────────────────────────────────────┐
│                    Inheritance vs Composition                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  C++/Java Inheritance:           Go Composition:                 │
│                                                                  │
│       Animal                        ┌─────────┐                  │
│         │                          │ Speaker │                  │
│    ┌────┴────┐                     └────┬────┘                  │
│    │         │                          │                        │
│  Dog       Cat                     ┌────┴────┐                  │
│    │                               │         │                   │
│ Labrador                         Dog       Cat                   │
│                                  ┌───┐     ┌───┐                 │
│  Problems:                       │   │     │   │                 │
│  - What if Dog needs             │ L │     │ * │                 │
│    multiple parents?             │ e │     │   │                 │
│  - Changing Animal               │ g │     └───┘                 │
│    breaks all children           │ s │                           │
│  - Deep hierarchies              └───┘                           │
│    are hard to follow                                            │
│                                  Dog HAS legs (composition)      │
│                                  Dog CAN speak (interface)       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
继承（inheritance）创建紧密耦合的代码层次结构，容易产生"脆弱基类"问题。Go 选择组合（composition）而非继承：将小的、独立的类型组合成更大的类型。这更灵活，更容易理解和修改。

## 2. Core Idea and Mental Model

Go's approach: **"Has-a" relationships, not "is-a" relationships.**

- No `class` keyword, no `extends`, no `implements`
- Structs contain data (fields)
- Methods are defined separately from struct definitions
- Embedding provides syntactic convenience (not true inheritance)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Go Struct Mental Model                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  A struct is just a collection of named fields:                  │
│                                                                  │
│  type Person struct {                                            │
│      Name string      ←── field: type                            │
│      Age  int                                                    │
│  }                                                               │
│                                                                  │
│  Memory layout (contiguous):                                     │
│  ┌────────────────────┬──────────────┐                          │
│  │ Name (string)      │ Age (int)    │                          │
│  │ [ptr][len]         │ [value]      │                          │
│  └────────────────────┴──────────────┘                          │
│    16 bytes              8 bytes                                 │
│                                                                  │
│  Embedding (composition, not inheritance):                       │
│                                                                  │
│  type Employee struct {                                          │
│      Person          ←── embedded (promotes Person's fields)     │
│      EmployeeID int                                              │
│  }                                                               │
│                                                                  │
│  Memory layout:                                                  │
│  ┌────────────────────┬──────────────┬──────────────┐           │
│  │ Person.Name        │ Person.Age   │ EmployeeID   │           │
│  └────────────────────┴──────────────┴──────────────┘           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Struct Definition

```go
// Basic struct
type Point struct {
    X, Y float64
}

// Struct with various field types
type Person struct {
    FirstName string
    LastName  string
    Age       int
    Email     string   `json:"email"`  // struct tag
    private   bool     // unexported field
}
```

### Struct Initialization

```go
// Zero value
var p1 Point  // {0, 0}

// Literal with field names (preferred)
p2 := Point{X: 10, Y: 20}

// Literal without field names (fragile, avoid)
p3 := Point{10, 20}

// Partial initialization (other fields get zero values)
p4 := Person{FirstName: "Alice", Age: 30}

// Pointer to struct
p5 := &Point{X: 5, Y: 10}
```

### Embedding (Composition)

```go
// Address is a separate, reusable type
type Address struct {
    Street  string
    City    string
    Country string
}

// Person embeds Address
type Person struct {
    Name    string
    Address         // Embedded (no field name)
}

// Usage: fields are "promoted"
p := Person{
    Name: "Alice",
    Address: Address{
        Street:  "123 Main St",
        City:    "Seattle",
        Country: "USA",
    },
}

fmt.Println(p.City)         // Promoted access
fmt.Println(p.Address.City) // Explicit access (also works)
```

### Method Promotion

```go
type Logger struct{}

func (l Logger) Log(msg string) {
    fmt.Println("LOG:", msg)
}

type Server struct {
    Logger  // Embed Logger
    Port    int
}

// Server "inherits" Log method through embedding
s := Server{Port: 8080}
s.Log("Starting")  // Calls Logger.Log
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
// Separate, focused structs for different address types
type RouterAddress struct {
    VrfId     uint32
    Address   string
    PrefixLen uint32
    IsV6      bool
}

type VmcAddress struct {
    VrfId     uint32
    VmcName   string    // Additional field specific to VMC
    Address   string
    PrefixLen uint32
    IsV6      bool
}

type VmcRoute struct {
    VrfId     uint32
    Address   string
    PrefixLen uint32
    NextHop   string    // Route-specific field
    IsV6      bool
}
```

Notice how these could share a common embedded type:

```go
// Alternative design using embedding
type BaseAddress struct {
    VrfId     uint32
    Address   string
    PrefixLen uint32
    IsV6      bool
}

type RouterAddress struct {
    BaseAddress
}

type VmcAddress struct {
    BaseAddress
    VmcName string
}

type VmcRoute struct {
    BaseAddress
    NextHop string
}

// Now all types share BaseAddress behavior
```

### gRPC Server Embedding

```go
type routermgrServer struct {
    routermgrpb.UnimplementedRouterMgrServer  // Embedded
}

// This embedding provides forward compatibility:
// - If new RPC methods are added to the interface,
//   the embedded type provides default implementations
// - Your server only implements methods you care about
```

## 5. Common Mistakes and Pitfalls

1. **Expecting inheritance behavior**:
   ```go
   type Base struct{}
   func (b Base) Name() string { return "Base" }
   func (b Base) Greet() { fmt.Println("Hello, I am", b.Name()) }
   
   type Derived struct{ Base }
   func (d Derived) Name() string { return "Derived" }
   
   d := Derived{}
   d.Greet()  // Prints "Hello, I am Base" - NOT polymorphic!
   
   // Base.Greet() calls Base.Name(), not Derived.Name()
   // Embedding is NOT inheritance
   ```

2. **Embedding pointers incorrectly**:
   ```go
   type Outer struct {
       *Inner  // Embedded pointer - nil by default!
   }
   
   o := Outer{}
   o.DoSomething()  // PANIC: nil pointer
   
   // Must initialize
   o := Outer{Inner: &Inner{}}
   ```

3. **Name conflicts with embedding**:
   ```go
   type A struct{ Name string }
   type B struct{ Name string }
   type C struct {
       A
       B
   }
   
   c := C{}
   c.Name  // Compile error: ambiguous selector
   
   // Must use explicit path
   c.A.Name
   c.B.Name
   ```

4. **Using embedding for code reuse only**:
   ```go
   // Bad: embedding just to reuse methods
   type UserService struct {
       Database  // Exposes all Database methods!
   }
   
   // Better: composition without embedding
   type UserService struct {
       db Database  // unexported, encapsulated
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C++ | Go |
|--------|-----|-----|
| Type definition | `class` with inheritance | `struct` only |
| Inheritance | `class D : public B` | No inheritance |
| Composition | Member variables | Embedding + fields |
| Virtual methods | `virtual` keyword | Interfaces (separate) |
| Constructors | `Class()` | No constructors |
| Access control | `public/private/protected` | Exported/unexported |

### C Struct (Linux Kernel Style)

```c
// Linux kernel: composition via nested structs
struct device {
    struct kobject kobj;
    const char *name;
    struct device *parent;
    // ...
};

// "Method" via function pointer
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    // ...
};
```

### Go Equivalent

```go
type Device struct {
    KObj   KObject
    Name   string
    Parent *Device
}

type FileOperations struct {
    Read  func(f *File, buf []byte, offset int64) (int, error)
    Write func(f *File, buf []byte, offset int64) (int, error)
}

// Or using interface
type FileOperations interface {
    Read(buf []byte, offset int64) (int, error)
    Write(buf []byte, offset int64) (int, error)
}
```

Go's approach is closer to C than C++ - structs contain data, behavior is separate.

## 7. A Small But Complete Go Example

```go
// composition.go - Demonstrating composition over inheritance
package main

import (
    "fmt"
    "time"
)

// Small, focused types

type Coordinates struct {
    Lat, Lon float64
}

func (c Coordinates) String() string {
    return fmt.Sprintf("(%.4f, %.4f)", c.Lat, c.Lon)
}

type Timestamps struct {
    CreatedAt time.Time
    UpdatedAt time.Time
}

func (t *Timestamps) Touch() {
    t.UpdatedAt = time.Now()
}

// Compose into larger types

type Location struct {
    Name string
    Coordinates  // Embedded
    Timestamps   // Embedded
}

type User struct {
    ID    int64
    Name  string
    Email string
    Timestamps  // Reuse timestamps
}

type Package struct {
    TrackingID   string
    Origin       Location
    Destination  Location
    CurrentPos   Coordinates
    Timestamps
}

func main() {
    // Location uses both Coordinates and Timestamps
    loc := Location{
        Name:        "Seattle HQ",
        Coordinates: Coordinates{Lat: 47.6062, Lon: -122.3321},
        Timestamps: Timestamps{
            CreatedAt: time.Now(),
            UpdatedAt: time.Now(),
        },
    }
    
    // Promoted method from Coordinates
    fmt.Println("Location:", loc.String())
    
    // Promoted method from Timestamps
    loc.Touch()
    
    // Promoted field access
    fmt.Printf("Latitude: %.4f\n", loc.Lat)
    fmt.Printf("Updated: %v\n", loc.UpdatedAt)
    
    // User reuses Timestamps
    user := User{
        ID:    1,
        Name:  "Alice",
        Email: "alice@example.com",
        Timestamps: Timestamps{
            CreatedAt: time.Now(),
        },
    }
    user.Touch()
    fmt.Printf("\nUser created: %v\n", user.CreatedAt)
    
    // Package composes multiple types
    pkg := Package{
        TrackingID: "PKG-12345",
        Origin: Location{
            Name:        "Warehouse A",
            Coordinates: Coordinates{Lat: 47.6, Lon: -122.3},
        },
        Destination: Location{
            Name:        "Customer Address",
            Coordinates: Coordinates{Lat: 37.7, Lon: -122.4},
        },
        CurrentPos: Coordinates{Lat: 45.5, Lon: -122.6},
    }
    pkg.Touch()
    
    fmt.Printf("\nPackage %s\n", pkg.TrackingID)
    fmt.Printf("  From: %s %s\n", pkg.Origin.Name, pkg.Origin.Coordinates)
    fmt.Printf("  To: %s %s\n", pkg.Destination.Name, pkg.Destination.Coordinates)
    fmt.Printf("  Current: %s\n", pkg.CurrentPos)
}
```

Output:
```
Location: (47.6062, -122.3321)
Latitude: 47.6062
Updated: 2024-01-15 10:30:00 +0000 UTC

User created: 2024-01-15 10:30:00 +0000 UTC

Package PKG-12345
  From: Warehouse A (47.6000, -122.3000)
  To: Customer Address (37.7000, -122.4000)
  Current: (45.5000, -122.6000)
```

**Key takeaways**:
- `Coordinates` and `Timestamps` are small, reusable types
- `Location`, `User`, `Package` compose them as needed
- Methods and fields are promoted through embedding
- No inheritance hierarchy, no fragile base class problem
- Each type can embed only what it needs

---

**Summary**: Go uses composition instead of inheritance. Structs are collections of fields. Embedding provides method and field promotion for convenience, but it's not polymorphic inheritance. This approach produces more flexible, loosely-coupled designs that are easier to understand and modify than deep class hierarchies.

