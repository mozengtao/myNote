# Topic 53: Common Beginner Traps

## 1. Loop Variable Capture

```go
// ❌ BUG: All goroutines print 10
for i := 0; i < 10; i++ {
    go func() {
        fmt.Println(i)
    }()
}

// ✅ FIX: Pass as parameter
for i := 0; i < 10; i++ {
    go func(n int) {
        fmt.Println(n)
    }(i)
}

// ✅ FIX (Go 1.22+): Fixed automatically
```

## 2. Nil Map Write

```go
// ❌ PANIC: assignment to nil map
var m map[string]int
m["key"] = 1

// ✅ FIX: Initialize first
m := make(map[string]int)
m["key"] = 1
```

## 3. Defer in Loop

```go
// ❌ BUG: Files not closed until function returns
for _, f := range files {
    file, _ := os.Open(f)
    defer file.Close()  // All defers queue up!
}

// ✅ FIX: Wrap in function
for _, f := range files {
    func() {
        file, _ := os.Open(f)
        defer file.Close()
        // Process file
    }()
}
```

## 4. Slice Gotcha

```go
// ❌ BUG: Modifying subslice affects original
original := []int{1, 2, 3, 4, 5}
subslice := original[:3]
subslice[0] = 999
fmt.Println(original)  // [999 2 3 4 5]

// ✅ FIX: Copy if needed
subslice := make([]int, 3)
copy(subslice, original[:3])
```

## 5. Interface Nil Check

```go
// ❌ BUG: err is non-nil even when *MyError is nil
func returnsError() error {
    var err *MyError = nil
    return err  // Returns non-nil interface!
}

// ✅ FIX: Return nil directly
func returnsError() error {
    var err *MyError = nil
    if err == nil {
        return nil
    }
    return err
}
```

## 6. Ignoring Errors

```go
// ❌ BUG: Error ignored
data, _ := ioutil.ReadFile("config.json")
json.Unmarshal(data, &config)  // Also ignoring error!

// ✅ FIX: Check every error
data, err := ioutil.ReadFile("config.json")
if err != nil {
    return fmt.Errorf("read config: %w", err)
}
```

## 7. String Equality with ==

```go
// This works in Go (unlike Java)
if s1 == s2 { }  // ✅ Correct

// But for byte slices, use bytes.Equal
if bytes.Equal(b1, b2) { }
```

## 8. Mutex Copy

```go
// ❌ BUG: Copying mutex
type Counter struct {
    mu    sync.Mutex
    count int
}

c1 := Counter{}
c2 := c1  // Copies mutex - undefined behavior!

// ✅ FIX: Use pointer
func process(c *Counter) { }
```

---

**Summary**: Watch for loop variable capture, nil maps, defer in loops, slice sharing, interface nil, and mutex copying. Use race detector and linters.

