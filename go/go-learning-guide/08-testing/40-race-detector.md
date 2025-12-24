# Topic 40: Race Detector

## Usage

```bash
go test -race ./...
go run -race main.go
go build -race -o myapp
```

## Example Race

```go
func TestRace(t *testing.T) {
    var count int
    var wg sync.WaitGroup
    
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            count++  // DATA RACE!
        }()
    }
    wg.Wait()
}
```

## Output

```
WARNING: DATA RACE
Write at 0x00c000018090:
  goroutine 7 at main.go:15

Previous write at 0x00c000018090:
  goroutine 6 at main.go:15
```

## Fix

```go
var count int64
atomic.AddInt64(&count, 1)  // Thread-safe
```

---

**Summary**: Always run `go test -race` in CI. Fix all races—they cause unpredictable bugs.

