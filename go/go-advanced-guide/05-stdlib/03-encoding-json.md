# encoding/json: Struct Tags and Marshaling

## 1. Engineering Problem

### What real-world problem does this solve?

**JSON serialization must handle the mismatch between Go's type system and JSON's dynamic nature.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    JSON SERIALIZATION CHALLENGES                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Go struct:                        JSON:                               │
│   ──────────                        ─────                               │
│   type Route struct {               {                                   │
│       VrfID   uint32                  "vrf_id": 1,                      │
│       Prefix  string                  "prefix": "10.0.0.0/24",          │
│       NextHop string                  "next_hop": "192.168.1.1"         │
│   }                                 }                                   │
│                                                                         │
│   Problems:                                                             │
│   • Go uses PascalCase, JSON uses snake_case                            │
│   • Some fields should be omitted if empty                              │
│   • Some fields should never be serialized (passwords, internal)        │
│   • Need to handle missing/null fields                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### What goes wrong when misunderstood?

- Fields silently ignored (unexported fields)
- Wrong field names in JSON output
- Zero values present when should be omitted
- Sensitive data accidentally serialized

---

## 2. Core Mental Model

### How Go expects you to think

**Struct tags are metadata that encoding/json reads at runtime via reflection.**

```go
type Route struct {
    VrfID   uint32 `json:"vrf_id"`           // Rename to snake_case
    Prefix  string `json:"prefix"`            // Same name, explicit
    NextHop string `json:"next_hop,omitempty"` // Omit if empty
    secret  string `json:"-"`                 // Never serialize
}
```

### Tag syntax

```
`json:"fieldname,option1,option2"`
```

| Option | Effect |
|--------|--------|
| `json:"name"` | Use custom field name |
| `json:",omitempty"` | Omit if zero value |
| `json:"-"` | Never include field |
| `json:",string"` | Encode number as quoted string |

### Philosophy

- Explicit over implicit
- Struct tags are the source of truth
- Unexported fields are private - never serialized

---

## 3. Language Mechanism

### Marshal (Go → JSON)

```go
route := Route{VrfID: 1, Prefix: "10.0.0.0/24"}

// To []byte
data, err := json.Marshal(route)
// {"vrf_id":1,"prefix":"10.0.0.0/24"}

// Pretty print
data, err := json.MarshalIndent(route, "", "  ")
```

### Unmarshal (JSON → Go)

```go
data := []byte(`{"vrf_id":1,"prefix":"10.0.0.0/24"}`)
var route Route
err := json.Unmarshal(data, &route)  // Pass pointer!
```

### Streaming with Encoder/Decoder

```go
// Encoder writes to io.Writer
func WriteRoutes(w io.Writer, routes []Route) error {
    encoder := json.NewEncoder(w)
    encoder.SetIndent("", "  ")
    return encoder.Encode(routes)
}

// Decoder reads from io.Reader
func ReadRoutes(r io.Reader) ([]Route, error) {
    var routes []Route
    decoder := json.NewDecoder(r)
    if err := decoder.Decode(&routes); err != nil {
        return nil, err
    }
    return routes, nil
}
```

### Custom MarshalJSON / UnmarshalJSON

```go
type IPAddress net.IP

func (ip IPAddress) MarshalJSON() ([]byte, error) {
    return json.Marshal(net.IP(ip).String())
}

func (ip *IPAddress) UnmarshalJSON(data []byte) error {
    var s string
    if err := json.Unmarshal(data, &s); err != nil {
        return err
    }
    *ip = IPAddress(net.ParseIP(s))
    return nil
}
```

---

## 4. Idiomatic Usage

### When to use

- API request/response bodies
- Configuration files
- Data interchange with other systems
- Logging structured data

### When NOT to use

- High-performance internal communication (use protobuf/msgpack)
- Large binary data (base64 is inefficient)
- Strictly typed schemas (consider protobuf)

### Common patterns

```go
// Separate API types from internal types
type RouteAPI struct {
    VrfID   uint32 `json:"vrf_id"`
    Prefix  string `json:"prefix"`
}

type Route struct {
    VrfID     uint32
    Prefix    string
    internal  string  // Not exposed
}

func (r Route) ToAPI() RouteAPI {
    return RouteAPI{VrfID: r.VrfID, Prefix: r.Prefix}
}
```

---

## 5. Common Pitfalls

### Pitfall 1: Unexported fields ignored

```go
// BAD: lowercase fields not serialized
type Route struct {
    vrfID  uint32  // Ignored!
    prefix string  // Ignored!
}

// GOOD: Export fields
type Route struct {
    VrfID  uint32 `json:"vrf_id"`
    Prefix string `json:"prefix"`
}
```

### Pitfall 2: Pointer nil vs struct zero

```go
type Response struct {
    Route *Route `json:"route,omitempty"`
}

// nil pointer: field omitted entirely
resp := Response{Route: nil}
// {}

// Zero struct pointer: field present as empty object
resp := Response{Route: &Route{}}
// {"route":{"vrf_id":0,"prefix":""}}
```

### Pitfall 3: Unmarshal into wrong type

```go
// BAD: Unmarshal into value (no effect)
var route Route
json.Unmarshal(data, route)  // route unchanged!

// GOOD: Unmarshal into pointer
json.Unmarshal(data, &route)  // route populated
```

### Pitfall 4: Ignoring errors

```go
// BAD: Errors ignored
json.Unmarshal(data, &route)

// GOOD: Always check
if err := json.Unmarshal(data, &route); err != nil {
    return fmt.Errorf("parse route: %w", err)
}
```

---

## 6. Complete, Realistic Example

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "io"
    "net/http"
    "time"
)

type Route struct {
    VrfID     uint32    `json:"vrf_id"`
    Prefix    string    `json:"prefix"`
    NextHop   string    `json:"next_hop,omitempty"`
    CreatedAt time.Time `json:"created_at"`
    UpdatedAt time.Time `json:"updated_at,omitempty"`
    internal  string    // Never serialized
}

type APIResponse struct {
    Success bool        `json:"success"`
    Data    interface{} `json:"data,omitempty"`
    Error   string      `json:"error,omitempty"`
}

type RouteService struct {
    routes map[string]Route
}

func NewRouteService() *RouteService {
    return &RouteService{routes: make(map[string]Route)}
}

// Handler for POST /routes
func (s *RouteService) AddRoute(w http.ResponseWriter, r *http.Request) {
    // Decode request body
    var route Route
    decoder := json.NewDecoder(r.Body)
    decoder.DisallowUnknownFields()  // Strict parsing
    
    if err := decoder.Decode(&route); err != nil {
        s.writeError(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
        return
    }
    
    // Validate
    if route.Prefix == "" {
        s.writeError(w, http.StatusBadRequest, "prefix required")
        return
    }
    
    // Set timestamps
    route.CreatedAt = time.Now()
    route.internal = "server-generated"
    
    // Store
    key := fmt.Sprintf("%d:%s", route.VrfID, route.Prefix)
    s.routes[key] = route
    
    // Respond
    s.writeJSON(w, http.StatusCreated, APIResponse{
        Success: true,
        Data:    route,
    })
}

// Handler for GET /routes
func (s *RouteService) ListRoutes(w http.ResponseWriter, r *http.Request) {
    routes := make([]Route, 0, len(s.routes))
    for _, r := range s.routes {
        routes = append(routes, r)
    }
    
    s.writeJSON(w, http.StatusOK, APIResponse{
        Success: true,
        Data:    routes,
    })
}

func (s *RouteService) writeJSON(w http.ResponseWriter, status int, v interface{}) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    
    encoder := json.NewEncoder(w)
    encoder.SetIndent("", "  ")
    if err := encoder.Encode(v); err != nil {
        // Log error but can't change response now
        fmt.Printf("JSON encode error: %v\n", err)
    }
}

func (s *RouteService) writeError(w http.ResponseWriter, status int, msg string) {
    s.writeJSON(w, status, APIResponse{
        Success: false,
        Error:   msg,
    })
}

// Client-side usage
func fetchRoutes(url string) ([]Route, error) {
    resp, err := http.Get(url)
    if err != nil {
        return nil, fmt.Errorf("fetch: %w", err)
    }
    defer resp.Body.Close()
    
    // Read and decode
    var apiResp APIResponse
    if err := json.NewDecoder(resp.Body).Decode(&apiResp); err != nil {
        return nil, fmt.Errorf("decode: %w", err)
    }
    
    if !apiResp.Success {
        return nil, fmt.Errorf("API error: %s", apiResp.Error)
    }
    
    // Type assertion from interface{}
    data, err := json.Marshal(apiResp.Data)
    if err != nil {
        return nil, err
    }
    
    var routes []Route
    if err := json.Unmarshal(data, &routes); err != nil {
        return nil, fmt.Errorf("unmarshal routes: %w", err)
    }
    
    return routes, nil
}

func main() {
    // Marshal example
    route := Route{
        VrfID:     1,
        Prefix:    "10.0.0.0/24",
        NextHop:   "192.168.1.1",
        CreatedAt: time.Now(),
        internal:  "secret",  // Won't be serialized
    }
    
    data, _ := json.MarshalIndent(route, "", "  ")
    fmt.Println("Marshaled:")
    fmt.Println(string(data))
    
    // Unmarshal example
    jsonData := `{"vrf_id":2,"prefix":"172.16.0.0/16","created_at":"2024-01-01T00:00:00Z"}`
    var parsed Route
    json.Unmarshal([]byte(jsonData), &parsed)
    fmt.Printf("\nUnmarshaled: %+v\n", parsed)
    
    // Streaming
    var buf bytes.Buffer
    json.NewEncoder(&buf).Encode(route)
    fmt.Printf("\nStreamed: %s", buf.String())
}
```

---

## 7. Design Takeaways

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    JSON DESIGN RULES                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. USE EXPLICIT TAGS                                                  │
│      • Always specify json:"name" for API types                         │
│      • Don't rely on default field name conversion                      │
│                                                                         │
│   2. SEPARATE API TYPES FROM INTERNAL TYPES                             │
│      • API types have json tags                                         │
│      • Internal types are implementation detail                         │
│                                                                         │
│   3. USE ENCODER/DECODER FOR STREAMS                                    │
│      • HTTP bodies                                                      │
│      • Files                                                            │
│      • Avoids loading entire content into memory                        │
│                                                                         │
│   4. HANDLE ERRORS                                                      │
│      • Check Marshal/Unmarshal errors                                   │
│      • Use DisallowUnknownFields() for strict parsing                   │
│                                                                         │
│   5. USE omitempty WISELY                                               │
│      • Optional response fields                                         │
│      • But be careful with zero values that are valid                   │
│                                                                         │
│   6. CUSTOM MARSHALING FOR COMPLEX TYPES                                │
│      • time.Time has built-in support                                   │
│      • net.IP needs custom marshaler                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Chinese Explanation (中文解释)

### JSON 结构体标签

| 标签 | 效果 |
|------|------|
| `json:"name"` | 自定义字段名 |
| `json:",omitempty"` | 零值时省略 |
| `json:"-"` | 跳过字段 |
| `json:",string"` | 数字编码为字符串 |

### 常见陷阱

1. **未导出字段被忽略**：小写字段不会序列化
2. **指针 nil vs 结构体零值**：行为不同
3. **忘记传指针**：`Unmarshal(data, &route)` 需要 `&`
4. **忽略错误**：总是检查返回的 error

### 最佳实践

- 使用显式标签
- API 类型和内部类型分离
- 用 Encoder/Decoder 处理流
- 总是检查错误
