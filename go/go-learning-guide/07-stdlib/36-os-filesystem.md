# Topic 36: Filesystem and OS Interaction

## 1. File Operations

```go
import "os"

// Create/Open
f, err := os.Create("new.txt")      // Create or truncate
f, err := os.Open("existing.txt")   // Read-only
f, err := os.OpenFile("log.txt", os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0644)

// Read/Write
data, err := os.ReadFile("config.json")
err := os.WriteFile("output.txt", data, 0644)

// Close
defer f.Close()
```

## 2. Directory Operations

```go
// Create
err := os.Mkdir("dir", 0755)
err := os.MkdirAll("path/to/nested/dir", 0755)

// Remove
err := os.Remove("file.txt")
err := os.RemoveAll("dir")

// List
entries, err := os.ReadDir(".")
for _, entry := range entries {
    info, _ := entry.Info()
    fmt.Println(entry.Name(), info.Size())
}
```

## 3. Path Operations

```go
import "path/filepath"

// Join paths (OS-aware)
path := filepath.Join("dir", "subdir", "file.txt")

// Get parts
dir := filepath.Dir(path)
base := filepath.Base(path)
ext := filepath.Ext(path)

// Walk directory tree
filepath.Walk(".", func(path string, info fs.FileInfo, err error) error {
    if err != nil {
        return err
    }
    fmt.Println(path)
    return nil
})
```

## 4. Environment

```go
// Get/Set
value := os.Getenv("HOME")
os.Setenv("MY_VAR", "value")

// Check existence
value, exists := os.LookupEnv("MY_VAR")
```

## 5. Process Execution

```go
import "os/exec"

// Simple command
out, err := exec.Command("ls", "-la").Output()

// With context (timeout/cancel)
ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
defer cancel()
out, err := exec.CommandContext(ctx, "long-running-cmd").Output()

// Combined output (stdout + stderr)
cmd := exec.Command("make", "build")
output, _ := cmd.CombinedOutput()
```

## 6. From routermgr_grpc.go

```go
// Executing system commands
cmd := exec.Command("/usr/sbin/ip", "addr", operStr, dummyPrefix, "dev", interfaceStr)
output, _ := cmd.CombinedOutput()
log.Debugf("%s IP Addr Command Output: %s", LogOperStr, string(output))
```

---

**Summary**: os package handles files, directories, environment. os/exec runs external commands. filepath handles paths portably.

