# Topic 35: io.Reader / io.Writer Interfaces

## 1. Problem It Solves

Unified abstraction for reading/writing data from any source/destination.

## 2. Core Interfaces

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

type Closer interface {
    Close() error
}

// Composed interfaces
type ReadWriter interface {
    Reader
    Writer
}

type ReadCloser interface {
    Reader
    Closer
}
```

## 3. Everything Implements These

```go
// Files
file, _ := os.Open("data.txt")  // implements io.ReadCloser

// Buffers
var buf bytes.Buffer  // implements io.ReadWriter

// Network
conn, _ := net.Dial("tcp", "host:80")  // implements io.ReadWriteCloser

// HTTP
resp, _ := http.Get(url)
body := resp.Body  // implements io.ReadCloser

// Strings
r := strings.NewReader("hello")  // implements io.Reader
```

## 4. Utility Functions

```go
// Copy all data
n, err := io.Copy(dst, src)

// Read all to memory
data, err := io.ReadAll(r)

// Limit reader
limited := io.LimitReader(r, 1024)

// Combine readers
multi := io.MultiReader(r1, r2, r3)

// Duplicate writes
tee := io.TeeReader(r, w)  // Writes to w as it reads from r
```

## 5. Practical Example

```go
func ProcessData(r io.Reader) error {
    // Works with files, HTTP bodies, buffers, anything!
    data, err := io.ReadAll(r)
    if err != nil {
        return err
    }
    // Process data...
    return nil
}

// Usage
ProcessData(os.Stdin)
ProcessData(resp.Body)
ProcessData(strings.NewReader("test"))
ProcessData(&buf)
```

## 6. From routermgr Context

```go
// exec.Command uses io interfaces
cmd := exec.Command("/usr/sbin/ip", "addr", operStr, dummyPrefix, "dev", interfaceStr)
output, _ := cmd.CombinedOutput()  // Returns []byte from stdout/stderr
```

---

**Summary**: io.Reader and io.Writer are Go's universal data transfer abstractions. Accept them in functions for maximum flexibility. Many stdlib functions work with these interfaces.

