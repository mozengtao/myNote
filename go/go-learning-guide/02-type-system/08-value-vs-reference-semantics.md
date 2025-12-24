# Topic 8: Value Semantics vs Reference Semantics

## 1. Problem It Solves (Engineering Motivation)

Understanding data mutation and sharing is critical for:
- **Correctness**: Does modifying this value affect other references?
- **Performance**: Copying vs sharing trade-offs
- **Concurrency**: Safe sharing between goroutines
- **Memory efficiency**: When does copying happen?

```
┌─────────────────────────────────────────────────────────────────┐
│                 Value vs Reference Semantics                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Value Semantics:                Reference Semantics:            │
│                                                                  │
│  a := Point{X: 1, Y: 2}         a := []int{1, 2, 3}             │
│  b := a                          b := a                          │
│  b.X = 10                        b[0] = 10                       │
│                                                                  │
│  ┌───────┐   ┌───────┐          ┌───┐   ┌───┐                   │
│  │ a     │   │ b     │          │ a │   │ b │                   │
│  │ X: 1  │   │ X: 10 │          └─┬─┘   └─┬─┘                   │
│  │ Y: 2  │   │ Y: 2  │            │       │                     │
│  └───────┘   └───────┘            └───┬───┘                     │
│                                       ▼                          │
│  Two independent copies         ┌──────────┐                    │
│  (a unchanged)                  │ [10,2,3] │                    │
│                                 └──────────┘                    │
│                                  Both see change                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**中文解释**：
Go 中有两种语义：值语义（赋值时复制整个值）和引用语义（赋值时共享底层数据）。理解这个区别对于写正确的代码至关重要。结构体和数组使用值语义，切片、map、channel 使用引用语义。

## 2. Core Idea and Mental Model

**Rule of thumb**:
- **Value types** (copied on assignment): `bool`, numbers, `string`, arrays, structs
- **Reference types** (shared on assignment): slices, maps, channels, functions, interfaces

But it's more nuanced: **Everything in Go is passed by value, but some types contain references.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    What Gets Copied                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Type          │ Assignment Copies      │ Underlying Data       │
│  ─────────────────────────────────────────────────────────────  │
│  int           │ The value itself       │ No underlying data    │
│  [3]int        │ All 3 elements         │ No underlying data    │
│  struct{A int} │ All fields             │ No underlying data    │
│  string        │ Header (ptr, len)      │ Shared (immutable)    │
│  []int         │ Header (ptr, len, cap) │ Shared (mutable!)     │
│  map[K]V       │ Pointer to hashmap     │ Shared (mutable!)     │
│  chan T        │ Pointer to channel     │ Shared                │
│  *T            │ Pointer value          │ Shared (mutable!)     │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │            Slice Header (24 bytes on 64-bit)            │    │
│  │  ┌────────────┬────────────┬────────────┐              │    │
│  │  │ ptr        │ len        │ cap        │              │    │
│  │  │ (8 bytes)  │ (8 bytes)  │ (8 bytes)  │              │    │
│  │  └─────┬──────┴────────────┴────────────┘              │    │
│  │        │                                                │    │
│  │        ▼                                                │    │
│  │  ┌───────────────────────────────────────┐             │    │
│  │  │  Backing Array [cap elements]         │             │    │
│  │  └───────────────────────────────────────┘             │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Go Language Features Involved

### Value Types

```go
// Struct - value type
type Point struct {
    X, Y int
}

a := Point{1, 2}
b := a        // Copy entire struct
b.X = 100     // a.X still 1

// Array - value type
arr1 := [3]int{1, 2, 3}
arr2 := arr1  // Copy all 3 elements
arr2[0] = 100 // arr1[0] still 1

// String - immutable value type
s1 := "hello"
s2 := s1     // Copy header, share bytes (safe because immutable)
```

### Reference Types

```go
// Slice - reference header, shared backing array
a := []int{1, 2, 3}
b := a        // Copy slice header, share backing array
b[0] = 100    // a[0] is now 100!

// Map - reference type
m1 := map[string]int{"a": 1}
m2 := m1      // Same map
m2["a"] = 100 // m1["a"] is now 100!

// Channel - reference type
ch1 := make(chan int)
ch2 := ch1    // Same channel
```

### Making Independent Copies

```go
// Slice: use copy() or append()
original := []int{1, 2, 3}
copied := make([]int, len(original))
copy(copied, original)
// Or: copied := append([]int{}, original...)

// Map: iterate and copy
original := map[string]int{"a": 1, "b": 2}
copied := make(map[string]int)
for k, v := range original {
    copied[k] = v
}
```

## 4. Typical Real-World Usage

From `routermgr_grpc.go`:

```go
// Struct fields - value semantics within struct
type VmcRoute struct {
    VrfId     uint32
    Address   string
    PrefixLen uint32
    NextHop   string
    IsV6      bool
}

// Map with struct as key - struct copied as key
VmcRoutes[route.VmcName][VmcRoute{
    VrfId:     vrfId,
    Address:   route.IpAddress,
    PrefixLen: route.PrefixLength,
    NextHop:   route.NextHopAddress,
    IsV6:      false,
}] = false

// Map - reference semantics
var VmcRoutes map[string]map[VmcRoute]bool
// All uses share the same map
// Changes from one place visible everywhere

// Careful: returned struct is a copy
routerIpv4Address, routerIpv4AddressExists := RouterAddresses[vrfId][Ipv4Idx]
// routerIpv4Address is a COPY of what's in the map
// Modifying it won't change the map
```

### When Semantics Matter

```go
// Function parameters - value vs reference
func ModifySlice(s []int) {
    s[0] = 999  // Modifies original!
}

func ModifyStruct(p Point) {
    p.X = 999  // Does NOT modify original
}

func ModifyStructPtr(p *Point) {
    p.X = 999  // Modifies original
}

// In real code
func (s *routermgrServer) AddRouteV4(_ context.Context, route *routermgrpb.AddIpv4Route) (*routermgrpb.RouteActionResponse, error) {
    // route is a pointer - we can read but convention says don't modify
    vrfId := route.VrfId  // Value copy
    
    // Modifying this local copy is fine
    if vrfId == InvalidVrfId {
        vrfId = DefaultVrfId  // Doesn't affect caller
    }
}
```

## 5. Common Mistakes and Pitfalls

1. **Modifying map values that are structs**:
   ```go
   type Entry struct {
       Count int
   }
   
   m := map[string]Entry{
       "a": {Count: 1},
   }
   
   // This FAILS: can't take address of map value
   m["a"].Count++  // Compile error!
   
   // Solution 1: Store pointers in map
   m := map[string]*Entry{"a": {Count: 1}}
   m["a"].Count++  // OK
   
   // Solution 2: Read, modify, write back
   entry := m["a"]
   entry.Count++
   m["a"] = entry  // OK
   ```

2. **Slice append gotcha**:
   ```go
   a := []int{1, 2, 3}
   b := a[:2]       // b shares backing array with a
   
   b = append(b, 4) // Might overwrite a[2]!
   // Result: a might be [1, 2, 4]
   
   // Safe: always capture append result
   b = append(b, 4)  // b might have new backing array
   ```

3. **Range loop variable sharing**:
   ```go
   items := []Item{{ID: 1}, {ID: 2}, {ID: 3}}
   
   // Bug: all goroutines share same 'item' variable
   for _, item := range items {
       go func() {
           process(item)  // All see last item!
       }()
   }
   
   // Fix: copy to local variable
   for _, item := range items {
       item := item  // Shadow with local copy
       go func() {
           process(item)  // Each gets its own copy
       }()
   }
   ```

4. **Expecting map iteration order**:
   ```go
   m := map[string]int{"a": 1, "b": 2, "c": 3}
   
   // Order is RANDOM and intentionally varies
   for k, v := range m {
       fmt.Println(k, v)  // Different order each run!
   }
   ```

## 6. How This Compares to C/C++ (or Linux Kernel Style)

| Aspect | C | Go |
|--------|---|-----|
| Struct assignment | Value copy | Value copy |
| Array assignment | Value copy | Value copy |
| Pointer assignment | Pointer copy (shares) | Pointer copy (shares) |
| Strings | char* (pointer) | Immutable value with sharing |
| Dynamic arrays | Manual (malloc + pointer) | Slice (header + backing) |
| Hash tables | Manual or library | Built-in map |

### C Memory Model

```c
// C: Everything is explicit
struct Point {
    int x, y;
};

void modify_value(struct Point p) {
    p.x = 100;  // Local copy, original unchanged
}

void modify_ptr(struct Point* p) {
    p->x = 100;  // Original modified
}

// Arrays decay to pointers
void modify_array(int arr[]) {
    arr[0] = 100;  // Original modified (arr is pointer)
}
```

### Go Equivalent

```go
// Go: Similar to C, but reference types are more nuanced
func modifyValue(p Point) {
    p.X = 100  // Local copy, original unchanged
}

func modifyPtr(p *Point) {
    p.X = 100  // Original modified
}

func modifySlice(s []int) {
    s[0] = 100  // Original modified (slice contains pointer)
    // BUT: s itself is a copy of the slice header
    s = append(s, 999)  // Doesn't affect caller's slice!
}
```

## 7. A Small But Complete Go Example

```go
// semantics.go - Value vs Reference semantics demonstration
package main

import "fmt"

type Point struct {
    X, Y int
}

type Data struct {
    Values []int
    Meta   map[string]string
}

func main() {
    // === Value Semantics ===
    fmt.Println("=== Value Semantics ===")
    
    // Struct
    p1 := Point{1, 2}
    p2 := p1
    p2.X = 100
    fmt.Printf("p1=%v, p2=%v (independent)\n", p1, p2)
    
    // Array
    arr1 := [3]int{1, 2, 3}
    arr2 := arr1
    arr2[0] = 100
    fmt.Printf("arr1=%v, arr2=%v (independent)\n", arr1, arr2)
    
    // === Reference Semantics ===
    fmt.Println("\n=== Reference Semantics ===")
    
    // Slice
    s1 := []int{1, 2, 3}
    s2 := s1
    s2[0] = 100
    fmt.Printf("s1=%v, s2=%v (shared!)\n", s1, s2)
    
    // Map
    m1 := map[string]int{"a": 1}
    m2 := m1
    m2["a"] = 100
    fmt.Printf("m1=%v, m2=%v (shared!)\n", m1, m2)
    
    // === Making True Copies ===
    fmt.Println("\n=== Making True Copies ===")
    
    // Slice copy
    s3 := []int{1, 2, 3}
    s4 := make([]int, len(s3))
    copy(s4, s3)
    s4[0] = 100
    fmt.Printf("s3=%v, s4=%v (independent)\n", s3, s4)
    
    // Map copy
    m3 := map[string]int{"a": 1, "b": 2}
    m4 := make(map[string]int)
    for k, v := range m3 {
        m4[k] = v
    }
    m4["a"] = 100
    fmt.Printf("m3=%v, m4=%v (independent)\n", m3, m4)
    
    // === Struct with reference fields ===
    fmt.Println("\n=== Struct with Reference Fields ===")
    
    d1 := Data{
        Values: []int{1, 2, 3},
        Meta:   map[string]string{"key": "value"},
    }
    d2 := d1  // Shallow copy!
    
    d2.Values[0] = 100
    d2.Meta["key"] = "modified"
    
    fmt.Printf("d1=%v (modified!)\n", d1)
    fmt.Printf("d2=%v\n", d2)
    
    // Deep copy needed for true independence
    d3 := Data{
        Values: make([]int, len(d1.Values)),
        Meta:   make(map[string]string),
    }
    copy(d3.Values, d1.Values)
    for k, v := range d1.Meta {
        d3.Meta[k] = v
    }
    d3.Values[0] = 999
    fmt.Printf("\nAfter deep copy:\n")
    fmt.Printf("d1=%v (unchanged)\n", d1)
    fmt.Printf("d3=%v (independent)\n", d3)
    
    // === Function Parameters ===
    fmt.Println("\n=== Function Parameters ===")
    
    point := Point{5, 5}
    modifyPoint(point)
    fmt.Printf("After modifyPoint: %v (unchanged)\n", point)
    
    modifyPointPtr(&point)
    fmt.Printf("After modifyPointPtr: %v (modified)\n", point)
    
    slice := []int{1, 2, 3}
    modifySlice(slice)
    fmt.Printf("After modifySlice: %v (element modified)\n", slice)
    
    extendSlice(slice)
    fmt.Printf("After extendSlice: %v (NOT extended - slice header copied)\n", slice)
}

func modifyPoint(p Point) {
    p.X = 999
}

func modifyPointPtr(p *Point) {
    p.X = 999
}

func modifySlice(s []int) {
    s[0] = 999  // Modifies backing array
}

func extendSlice(s []int) {
    s = append(s, 999)  // s is a copy of the header
    // Caller's slice doesn't see the append
}
```

Output:
```
=== Value Semantics ===
p1={1 2}, p2={100 2} (independent)
arr1=[1 2 3], arr2=[100 2 3] (independent)

=== Reference Semantics ===
s1=[100 2 3], s2=[100 2 3] (shared!)
m1=map[a:100], m2=map[a:100] (shared!)

=== Making True Copies ===
s3=[1 2 3], s4=[100 2 3] (independent)
m3=map[a:1 b:2], m4=map[a:100 b:2] (independent)

=== Struct with Reference Fields ===
d1={[100 2 3] map[key:modified]} (modified!)
d2={[100 2 3] map[key:modified]}

After deep copy:
d1={[100 2 3] map[key:modified]} (unchanged)
d3={[999 2 3] map[key:modified]} (independent)

=== Function Parameters ===
After modifyPoint: {5 5} (unchanged)
After modifyPointPtr: {999 5} (modified)
After modifySlice: [999 2 3] (element modified)
After extendSlice: [999 2 3] (NOT extended - slice header copied)
```

---

**Summary**: Go has consistent value semantics for all assignments, but some types (slices, maps, channels) contain internal pointers that are shared. Understanding this distinction is crucial for writing correct concurrent code and avoiding subtle mutation bugs. When in doubt, remember: structs and arrays are fully copied; slices and maps share their backing storage.

