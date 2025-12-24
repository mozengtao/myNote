# Topic 34: encoding/json and Data Marshaling

## 1. Problem It Solves

Converting between Go structs and JSON (and other formats).

## 2. Struct Tags

```go
type User struct {
    ID        int64     `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email,omitempty"`
    Password  string    `json:"-"`  // Never marshal
    CreatedAt time.Time `json:"created_at"`
}
```

## 3. Marshal and Unmarshal

```go
// Struct to JSON
user := User{ID: 1, Name: "Alice"}
data, err := json.Marshal(user)
// {"id":1,"name":"Alice","created_at":"0001-01-01T00:00:00Z"}

// JSON to struct
var user2 User
err = json.Unmarshal(data, &user2)

// Pretty print
data, err := json.MarshalIndent(user, "", "  ")
```

## 4. Streaming (Encoder/Decoder)

```go
// HTTP response
func handler(w http.ResponseWriter, r *http.Request) {
    user := User{ID: 1, Name: "Alice"}
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(user)
}

// HTTP request
func handler(w http.ResponseWriter, r *http.Request) {
    var user User
    if err := json.NewDecoder(r.Body).Decode(&user); err != nil {
        http.Error(w, err.Error(), http.StatusBadRequest)
        return
    }
}
```

## 5. Custom Marshaling

```go
type Status int

const (
    StatusPending Status = iota
    StatusActive
    StatusComplete
)

func (s Status) MarshalJSON() ([]byte, error) {
    names := []string{"pending", "active", "complete"}
    return json.Marshal(names[s])
}

func (s *Status) UnmarshalJSON(data []byte) error {
    var name string
    if err := json.Unmarshal(data, &name); err != nil {
        return err
    }
    switch name {
    case "pending":
        *s = StatusPending
    case "active":
        *s = StatusActive
    case "complete":
        *s = StatusComplete
    default:
        return fmt.Errorf("unknown status: %s", name)
    }
    return nil
}
```

## 6. Dynamic JSON

```go
// Unknown structure
var data map[string]interface{}
json.Unmarshal(jsonBytes, &data)

// Or use json.RawMessage for delayed parsing
type Envelope struct {
    Type    string          `json:"type"`
    Payload json.RawMessage `json:"payload"`
}
```

---

**Summary**: Use struct tags for field mapping, `json.Encoder`/`Decoder` for streaming, and implement `MarshalJSON`/`UnmarshalJSON` for custom types.

