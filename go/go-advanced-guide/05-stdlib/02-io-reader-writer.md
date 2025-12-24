# io.Reader and io.Writer: Composable I/O

## 1. Engineering Problem

### What real-world problem does this solve?

**io.Reader and io.Writer are Go's universal I/O abstractions that enable composition.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    COMPOSABLE I/O                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   type Reader interface {                                               │
│       Read(p []byte) (n int, err error)                                │
│   }                                                                     │
│                                                                         │
│   type Writer interface {                                               │
│       Write(p []byte) (n int, err error)                               │
│   }                                                                     │
│                                                                         │
│   ─────────────────────────────────────────────────────────────────────│
│                                                                         │
│   Composition example:                                                  │
│                                                                         │
│   File ──► gzip.Reader ──► bufio.Reader ──► json.Decoder               │
│      │          │               │                │                      │
│      ▼          ▼               ▼                ▼                      │
│   Read bytes   Decompress    Buffer          Parse JSON                 │
│                                                                         │
│   Each layer is an io.Reader wrapping another io.Reader                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Mental Model

### Reader contract

```go
// Read fills p with data
// Returns n bytes read and error
// EOF is signaled by io.EOF error
func (r *MyReader) Read(p []byte) (n int, err error) {
    // Read up to len(p) bytes
    // Return n = actual bytes read
    // Return io.EOF when done
}
```

### Writer contract

```go
// Write writes p to underlying destination
// Returns n bytes written and error
// Must return error if n < len(p)
func (w *MyWriter) Write(p []byte) (n int, err error) {
    // Write len(p) bytes
    // Return n = actual bytes written
    // Return error if cannot write all
}
```

---

## 3. Language Mechanism

### Common Readers

```go
// File
f, _ := os.Open("data.txt")
defer f.Close()

// Network
conn, _ := net.Dial("tcp", "localhost:8080")

// String
r := strings.NewReader("hello world")

// Bytes
r := bytes.NewReader([]byte{1, 2, 3})

// HTTP Response
resp, _ := http.Get("http://example.com")
defer resp.Body.Close()
```

### Common Writers

```go
// File
f, _ := os.Create("output.txt")
defer f.Close()

// Buffer
var buf bytes.Buffer

// HTTP Response
func handler(w http.ResponseWriter, r *http.Request) {
    w.Write([]byte("hello"))
}

// Network
conn, _ := net.Dial("tcp", "localhost:8080")
```

### Composing Readers/Writers

```go
// Buffered reading
r := bufio.NewReader(file)

// Compressed reading
gz, _ := gzip.NewReader(file)
defer gz.Close()

// Chained: compressed + buffered
gz, _ := gzip.NewReader(file)
r := bufio.NewReader(gz)

// Multi-writer: write to multiple destinations
w := io.MultiWriter(file, os.Stdout, &buf)
```

---

## 4. Idiomatic Usage

### Copy between Reader and Writer

```go
// Copy all data
n, err := io.Copy(dst, src)

// Copy with buffer
buf := make([]byte, 32*1024)
n, err := io.CopyBuffer(dst, src, buf)

// Copy exactly n bytes
n, err := io.CopyN(dst, src, 1024)
```

### Read all

```go
// Read entire contents
data, err := io.ReadAll(reader)

// Read exactly n bytes
buf := make([]byte, 1024)
_, err := io.ReadFull(reader, buf)
```

### Function accepting Reader/Writer

```go
// Accept interface, not concrete type
func ProcessData(r io.Reader) error {
    // Works with file, network, string, etc.
    data, err := io.ReadAll(r)
    if err != nil {
        return err
    }
    return process(data)
}

// Can be called with any Reader
ProcessData(file)
ProcessData(resp.Body)
ProcessData(strings.NewReader("data"))
```

---

## 5. Common Pitfalls

### Pitfall 1: Not checking returned n

```go
// BAD: Ignores partial read
buf := make([]byte, 1024)
r.Read(buf)  // May read less than 1024!

// GOOD: Check n and loop if needed
buf := make([]byte, 1024)
n, err := r.Read(buf)
data := buf[:n]  // Only use bytes actually read
```

### Pitfall 2: Ignoring io.EOF

```go
// BAD: Treats EOF as error
for {
    n, err := r.Read(buf)
    if err != nil {
        return err  // Returns io.EOF as error!
    }
}

// GOOD: Handle EOF specially
for {
    n, err := r.Read(buf)
    if n > 0 {
        process(buf[:n])
    }
    if err == io.EOF {
        break  // Normal end
    }
    if err != nil {
        return err
    }
}
```

---

## 6. Complete Example

```go
package main

import (
    "bufio"
    "bytes"
    "compress/gzip"
    "encoding/json"
    "fmt"
    "io"
    "strings"
)

type Route struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
    NextHop string `json:"next_hop"`
}

// ParseRoutes reads routes from any Reader
func ParseRoutes(r io.Reader) ([]Route, error) {
    var routes []Route
    decoder := json.NewDecoder(r)
    if err := decoder.Decode(&routes); err != nil {
        return nil, fmt.Errorf("decode routes: %w", err)
    }
    return routes, nil
}

// WriteRoutes writes routes to any Writer
func WriteRoutes(w io.Writer, routes []Route) error {
    encoder := json.NewEncoder(w)
    encoder.SetIndent("", "  ")
    return encoder.Encode(routes)
}

// CompressedReader wraps reader with gzip decompression
func CompressedReader(r io.Reader) (io.ReadCloser, error) {
    return gzip.NewReader(r)
}

// CountingWriter tracks bytes written
type CountingWriter struct {
    W     io.Writer
    Count int64
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.W.Write(p)
    cw.Count += int64(n)
    return n, err
}

func main() {
    // Parse routes from string
    jsonData := `[
        {"vrf_id": 1, "prefix": "10.0.0.0/24", "next_hop": "192.168.1.1"},
        {"vrf_id": 1, "prefix": "10.0.1.0/24", "next_hop": "192.168.1.2"}
    ]`
    
    routes, err := ParseRoutes(strings.NewReader(jsonData))
    if err != nil {
        fmt.Println("Error:", err)
        return
    }
    fmt.Printf("Parsed %d routes\n", len(routes))
    
    // Write to buffer with counting
    var buf bytes.Buffer
    cw := &CountingWriter{W: &buf}
    
    if err := WriteRoutes(cw, routes); err != nil {
        fmt.Println("Error:", err)
        return
    }
    fmt.Printf("Wrote %d bytes\n", cw.Count)
    
    // Buffered reading line by line
    scanner := bufio.NewScanner(strings.NewReader(buf.String()))
    for scanner.Scan() {
        fmt.Println("Line:", scanner.Text())
    }
    
    // Copy example
    var copy bytes.Buffer
    n, _ := io.Copy(&copy, strings.NewReader("data to copy"))
    fmt.Printf("Copied %d bytes\n", n)
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    IO INTERFACE RULES                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. ACCEPT io.Reader/io.Writer IN FUNCTIONS                            │
│      • Enables composition                                              │
│      • Works with any source/destination                                │
│                                                                         │
│   2. HANDLE PARTIAL READS                                               │
│      • Read may return n < len(p)                                       │
│      • Loop or use io.ReadFull                                          │
│                                                                         │
│   3. io.EOF IS NOT AN ERROR                                             │
│      • Signals normal end of data                                       │
│      • Check separately from errors                                     │
│                                                                         │
│   4. USE io.Copy FOR STREAMING                                          │
│      • No need to buffer in memory                                      │
│      • Efficient for large data                                         │
│                                                                         │
│   5. CLOSE WHEN DONE                                                    │
│      • Check for io.Closer                                              │
│      • defer Close() after successful open                              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### io.Reader 和 io.Writer

**Go 的通用 I/O 抽象，支持组合。**

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}
```

### 组合示例

```
File ──► gzip.Reader ──► bufio.Reader ──► json.Decoder
```

### 最佳实践

1. **函数接受 io.Reader/io.Writer**：支持任意来源
2. **处理部分读取**：Read 可能返回 n < len(p)
3. **io.EOF 不是错误**：正常结束信号
4. **用 io.Copy 流式处理**：无需内存缓冲
5. **完成后关闭**：检查 io.Closer

