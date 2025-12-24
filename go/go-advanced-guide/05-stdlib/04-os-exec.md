# os and os/exec: System Interaction

## 1. Engineering Problem

**Go provides os and os/exec for filesystem operations and external commands.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SYSTEM INTERACTION                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   os package:          os/exec package:                                 │
│   ──────────           ────────────────                                 │
│   • File operations    • Run external commands                          │
│   • Environment vars   • Capture stdout/stderr                          │
│   • Process info       • Pipe input/output                              │
│   • Signals            • Context cancellation                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. File Operations

```go
// Read file
data, err := os.ReadFile("config.json")

// Write file
err := os.WriteFile("output.txt", data, 0644)

// Open for reading
f, err := os.Open("file.txt")
defer f.Close()

// Create/truncate
f, err := os.Create("new.txt")
defer f.Close()
```

## 3. Running Commands

```go
// Simple run (like routermgr_grpc.go)
cmd := exec.Command("/usr/sbin/ip", "addr", "add", prefix, "dev", iface)
output, err := cmd.CombinedOutput()

// With context for timeout
ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
defer cancel()
cmd := exec.CommandContext(ctx, "ip", "route", "add", prefix)
err := cmd.Run()
```

## 4. Capturing Output

```go
// Capture stdout only
out, err := exec.Command("ls", "-la").Output()

// Capture stdout and stderr
cmd := exec.Command("ip", "addr")
var stdout, stderr bytes.Buffer
cmd.Stdout = &stdout
cmd.Stderr = &stderr
err := cmd.Run()
```

## 5. Common Pitfalls

```go
// BAD: No timeout
cmd := exec.Command("slow-command")
cmd.Run()  // May hang forever!

// GOOD: Use context
ctx, cancel := context.WithTimeout(ctx, 30*time.Second)
defer cancel()
cmd := exec.CommandContext(ctx, "slow-command")
```

---

## Chinese Explanation (中文解释)

### os 包
- 文件读写：`os.ReadFile`, `os.WriteFile`
- 环境变量：`os.Getenv`, `os.Setenv`

### os/exec 包
- 运行命令：`exec.Command`
- 超时控制：`exec.CommandContext`
- 捕获输出：`cmd.Output()`, `cmd.CombinedOutput()`

