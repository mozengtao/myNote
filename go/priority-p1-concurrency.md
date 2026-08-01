# Go 核心知识点代码示例 —— ⭐⭐⭐⭐☆ 并发与标准库

> 本文档拆分自 [core_concepts.md](./core_concepts.md) 中"80/20 学习优先级"表格的第二梯队。
>
> 这一梯队是现代 Go 服务开发的核心：**并发原语（Goroutine / Channel）、生命周期管理（Context）、同步机制（sync / Mutex / WaitGroup）、以及贯穿一切的标准库（io / os / net/http / json）**。

---

# 1. Goroutine、Channel

## 核心思想

```
Goroutine = 极轻量的任务单元（不是 OS 线程）
Channel   = goroutine 之间通信的管道

Don't communicate by sharing memory.
Share memory by communicating.
```

Go 的并发模型是 CSP（Communicating Sequential Processes）：不用共享变量 + 锁去同步，而是用 channel 传递数据。

## 为什么重要

- 是 Go 并发编程的核心，几乎所有高并发场景都基于此
- 掌握生产者消费者、Worker Pool 等模式，才能写出安全高效的并发程序
- 正确使用 channel 关闭 / `select`，是避免死锁、goroutine 泄漏的关键

## 代码示例

### 示例 1：启动 goroutine 的基本用法

```go
package main

import (
	"fmt"
	"sync"
)

func worker(id int, wg *sync.WaitGroup) {
	defer wg.Done()
	fmt.Printf("worker %d done\n", id)
}

func main() {
	var wg sync.WaitGroup
	for i := 1; i <= 3; i++ {
		wg.Add(1)
		go worker(i, &wg) // 启动goroutine，不等待其完成就继续往下走
	}
	wg.Wait() // 等待所有goroutine完成
	fmt.Println("all done")
}
```

### 示例 2：无缓冲 channel——同步传递数据

```go
package main

import "fmt"

func main() {
	ch := make(chan string) // 无缓冲：发送方会阻塞，直到有人接收

	go func() {
		ch <- "hello from goroutine"
	}()

	msg := <-ch
	fmt.Println(msg)
}
```

### 示例 3：有缓冲 channel 与容量

```go
package main

import "fmt"

func main() {
	ch := make(chan int, 3) // 缓冲区大小为3

	ch <- 1
	ch <- 2
	ch <- 3
	// ch <- 4 // 如果再发送，会阻塞，因为缓冲区已满

	fmt.Println(len(ch), cap(ch)) // 3 3
	fmt.Println(<-ch, <-ch, <-ch) // 1 2 3
}
```

### 示例 4：关闭 channel 与 range 消费

```go
package main

import "fmt"

func producer(ch chan<- int) {
	for i := 1; i <= 5; i++ {
		ch <- i
	}
	close(ch) // 生产完毕后必须关闭，否则range会永久阻塞
}

func main() {
	ch := make(chan int)
	go producer(ch)

	for v := range ch { // channel关闭后range自动退出循环
		fmt.Println(v)
	}
}
```

### 示例 5：select 多路复用与超时控制

```go
package main

import (
	"fmt"
	"time"
)

func main() {
	ch := make(chan string)

	go func() {
		time.Sleep(200 * time.Millisecond)
		ch <- "result"
	}()

	select {
	case res := <-ch:
		fmt.Println("got:", res)
	case <-time.After(100 * time.Millisecond):
		fmt.Println("timeout") // 会先触发，因为100ms < 200ms
	}
}
```

### 示例 6：Worker Pool 模式

```go
package main

import (
	"fmt"
	"sync"
)

func worker(id int, jobs <-chan int, results chan<- int, wg *sync.WaitGroup) {
	defer wg.Done()
	for j := range jobs {
		results <- j * j
	}
}

func main() {
	jobs := make(chan int, 10)
	results := make(chan int, 10)
	var wg sync.WaitGroup

	for w := 1; w <= 3; w++ { // 启动3个worker并发消费任务
		wg.Add(1)
		go worker(w, jobs, results, &wg)
	}

	for j := 1; j <= 5; j++ {
		jobs <- j
	}
	close(jobs)

	wg.Wait()
	close(results)

	for r := range results {
		fmt.Println(r)
	}
}
```

## 快速上手小结

需要"多个goroutine协作处理数据流"就用 channel；只是"等待一组goroutine跑完"就用 `sync.WaitGroup`；涉及超时/取消，`select` + `time.After` / `context` 是标准组合。

---

# 2. Context

## 核心思想

```
Context = 生命周期与取消信号的载体（一棵取消树）
```

`context.Context` 用来在 goroutine 之间传递 deadline、取消信号，以及少量请求范围的值。父 context 取消，所有派生的子 context 会级联取消。

## 为什么重要

- 现代 Go 服务（HTTP / RPC / DB）几乎所有阻塞调用的第一个参数都是 `context.Context`
- 统一的超时/取消机制，避免 goroutine 泄漏
- 父 context 取消会级联取消所有子 context，天然适合表达"请求生命周期"
- 是构建可靠、可观测服务的基础设施

## 代码示例

### 示例 1：context.Background + WithTimeout

```go
package main

import (
	"context"
	"fmt"
	"time"
)

func main() {
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel() // 无论超时与否，函数退出都应释放资源

	select {
	case <-time.After(200 * time.Millisecond):
		fmt.Println("work done")
	case <-ctx.Done():
		fmt.Println("timeout:", ctx.Err()) // context deadline exceeded
	}
}
```

### 示例 2：手动取消——WithCancel

```go
package main

import (
	"context"
	"fmt"
	"time"
)

func worker(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			fmt.Println("worker stopped:", ctx.Err())
			return
		default:
			time.Sleep(50 * time.Millisecond)
		}
	}
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	go worker(ctx)

	time.Sleep(150 * time.Millisecond)
	cancel() // 手动触发取消
	time.Sleep(100 * time.Millisecond)
}
```

### 示例 3：context 传值——只传请求范围元数据

```go
package main

import (
	"context"
	"fmt"
)

type ctxKey string

const requestIDKey ctxKey = "requestID"

func handle(ctx context.Context) {
	if id, ok := ctx.Value(requestIDKey).(string); ok {
		fmt.Println("handling request:", id)
	}
}

func main() {
	ctx := context.WithValue(context.Background(), requestIDKey, "req-123")
	handle(ctx)
}
```

### 示例 4：HTTP handler 中用 context 控制下游调用超时

```go
package main

import (
	"context"
	"fmt"
	"net/http"
	"time"
)

func slowQuery(ctx context.Context) error {
	select {
	case <-time.After(2 * time.Second):
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

func handler(w http.ResponseWriter, r *http.Request) {
	ctx, cancel := context.WithTimeout(r.Context(), 500*time.Millisecond)
	defer cancel()

	if err := slowQuery(ctx); err != nil {
		http.Error(w, err.Error(), http.StatusGatewayTimeout)
		return
	}
	fmt.Fprintln(w, "ok")
}
```

### 示例 5：级联取消——父取消，子全部取消

```go
package main

import (
	"context"
	"fmt"
	"time"
)

func main() {
	parent, cancelParent := context.WithCancel(context.Background())
	child, cancelChild := context.WithTimeout(parent, 5*time.Second)
	defer cancelChild()

	cancelParent() // 取消父context

	select {
	case <-child.Done():
		fmt.Println("child cancelled too:", child.Err()) // context canceled
	case <-time.After(1 * time.Second):
		fmt.Println("child still running")
	}
}
```

### 示例 6：常见陷阱——忘记调用 cancel

```go
package main

import (
	"context"
	"fmt"
	"time"
)

func badExample() {
	ctx, _ := context.WithTimeout(context.Background(), 1*time.Second)
	// 忘记 defer cancel()，即使 ctx 已经用不到了，
	// 相关的定时器资源也要等1秒后才会被自动释放
	_ = ctx
}

func goodExample() {
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
	defer cancel() // 正确做法：无论提前返回还是正常结束，都立即释放资源
	fmt.Println(ctx.Err())
}

func main() {
	badExample()
	goodExample()
}
```

## 快速上手小结

任何可能阻塞/耗时的函数，第一个参数都应该是 `context.Context`；创建 `WithTimeout`/`WithCancel` 之后，永远紧跟一个 `defer cancel()`。

---

# 3. sync、Mutex、WaitGroup

## 核心思想

```
sync 包 = 共享内存场景下的同步原语
WaitGroup = 等待一组 goroutine 完成
Mutex     = 保护共享数据的并发访问
```

不是所有并发场景都适合 channel，很多时候直接保护共享状态更简单直接。

## 为什么重要

- 数据竞争（data race）是并发 bug 里最隐蔽、最难调试的一类
- `go run -race` 是发现这些问题的必备工具
- `sync.Once`、`sync.Map` 等是常见的工程化并发工具
- 理解锁的粒度和读写分离（`RWMutex`），直接影响并发性能

## 代码示例

### 示例 1：WaitGroup 等待多个 goroutine 完成

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	var wg sync.WaitGroup
	results := make([]int, 5)

	for i := 0; i < 5; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			results[idx] = idx * idx
		}(i)
	}

	wg.Wait()
	fmt.Println(results) // [0 1 4 9 16]
}
```

### 示例 2：Mutex 保护共享计数器

```go
package main

import (
	"fmt"
	"sync"
)

type SafeCounter struct {
	mu    sync.Mutex
	count int
}

func (c *SafeCounter) Inc() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.count++
}

func main() {
	c := SafeCounter{}
	var wg sync.WaitGroup

	for i := 0; i < 1000; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			c.Inc()
		}()
	}

	wg.Wait()
	fmt.Println(c.count) // 1000，没有Mutex的话结果会小于1000（数据竞争）
}
```

### 示例 3：RWMutex——读多写少场景优化

```go
package main

import (
	"fmt"
	"sync"
)

type Cache struct {
	mu   sync.RWMutex
	data map[string]string
}

func (c *Cache) Get(key string) string {
	c.mu.RLock() // 多个读操作可以并发进行
	defer c.mu.RUnlock()
	return c.data[key]
}

func (c *Cache) Set(key, value string) {
	c.mu.Lock() // 写操作独占
	defer c.mu.Unlock()
	c.data[key] = value
}

func main() {
	cache := &Cache{data: make(map[string]string)}
	cache.Set("name", "gopher")
	fmt.Println(cache.Get("name")) // gopher
}
```

### 示例 4：sync.Once——只执行一次的初始化

```go
package main

import (
	"fmt"
	"sync"
)

var (
	once     sync.Once
	instance *Config
)

type Config struct {
	Name string
}

func GetConfig() *Config {
	once.Do(func() {
		fmt.Println("initializing config...")
		instance = &Config{Name: "default"}
	})
	return instance
}

func main() {
	for i := 0; i < 3; i++ {
		GetConfig() // "initializing config..." 只会打印一次
	}
}
```

### 示例 5：数据竞争示例——用 -race 检测

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	counter := 0
	var wg sync.WaitGroup

	for i := 0; i < 1000; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			counter++ // 数据竞争：多个goroutine同时读写counter，没有同步
		}()
	}

	wg.Wait()
	fmt.Println(counter) // 结果不确定，可能小于1000
	// 用 `go run -race main.go` 运行可以检测到这个数据竞争
}
```

### 示例 6：sync.Map——并发安全的 map

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	var m sync.Map
	var wg sync.WaitGroup

	for i := 0; i < 10; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			m.Store(n, n*n) // 无需额外加锁
		}(i)
	}
	wg.Wait()

	m.Range(func(key, value any) bool {
		fmt.Println(key, "->", value)
		return true
	})
}
```

## 快速上手小结

多个 goroutine 读写同一份数据，第一反应是"要不要加锁"；写完并发代码后习惯性跑一次 `go run -race`，比事后排查数据竞争便宜得多。

---

# 4. 标准库（io、os、net/http、json）

## 核心思想

```
Standard Library > Framework
```

Go 的设计哲学是"标准库优先"：`io` 提供统一的读写抽象，`os` 处理文件/环境/进程，`net/http` 内置生产可用的 HTTP 服务端/客户端，`encoding/json` 覆盖绝大多数序列化需求。

## 为什么重要

- 减少对第三方框架的依赖，代码更可控、更易维护
- `io.Reader` / `io.Writer` 是贯穿整个标准库的统一抽象
- `net/http` 内置了 production-ready 的 HTTP server/client
- `encoding/json` 是几乎所有 API 开发都要用到的序列化工具

## 代码示例

### 示例 1：io.Reader / io.Writer 统一抽象

```go
package main

import (
	"fmt"
	"io"
	"strings"
)

func countBytes(r io.Reader) (int, error) {
	buf := make([]byte, 1024)
	total := 0
	for {
		n, err := r.Read(buf)
		total += n
		if err == io.EOF {
			return total, nil
		}
		if err != nil {
			return total, err
		}
	}
}

func main() {
	r := strings.NewReader("hello, world")
	n, _ := countBytes(r)
	fmt.Println(n) // 12
}
```

### 示例 2：os 包——文件读写与环境变量

```go
package main

import (
	"fmt"
	"os"
)

func main() {
	err := os.WriteFile("demo.txt", []byte("hello go"), 0644)
	if err != nil {
		fmt.Println("write error:", err)
		return
	}
	defer os.Remove("demo.txt")

	data, err := os.ReadFile("demo.txt")
	if err != nil {
		fmt.Println("read error:", err)
		return
	}
	fmt.Println(string(data)) // hello go

	home := os.Getenv("HOME")
	fmt.Println("home dir:", home)
}
```

### 示例 3：bufio——按行读取

```go
package main

import (
	"bufio"
	"fmt"
	"strings"
)

func main() {
	text := "line1\nline2\nline3"
	scanner := bufio.NewScanner(strings.NewReader(text))

	for scanner.Scan() {
		fmt.Println("got:", scanner.Text())
	}
}
```

### 示例 4：net/http——最小 HTTP server

```go
package main

import (
	"fmt"
	"net/http"
)

func helloHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Hello, %s!\n", r.URL.Query().Get("name"))
}

func main() {
	http.HandleFunc("/hello", helloHandler)
	fmt.Println("listening on :8080")
	http.ListenAndServe(":8080", nil)
}
```

### 示例 5：net/http——最小 HTTP client

```go
package main

import (
	"fmt"
	"io"
	"net/http"
)

func main() {
	resp, err := http.Get("https://example.com")
	if err != nil {
		fmt.Println("request failed:", err)
		return
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	fmt.Println("status:", resp.Status)
	fmt.Println("bytes:", len(body))
}
```

### 示例 6：encoding/json——序列化与反序列化

```go
package main

import (
	"encoding/json"
	"fmt"
)

type User struct {
	Name  string `json:"name"`
	Age   int    `json:"age"`
	Email string `json:"email,omitempty"`
}

func main() {
	u := User{Name: "Alice", Age: 30}

	data, _ := json.Marshal(u)
	fmt.Println(string(data)) // {"name":"Alice","age":30}

	var u2 User
	json.Unmarshal([]byte(`{"name":"Bob","age":25,"email":"bob@example.com"}`), &u2)
	fmt.Printf("%+v\n", u2) // {Name:Bob Age:25 Email:bob@example.com}
}
```

## 快速上手小结

写业务代码前先问一句"标准库有没有现成的"；`io.Reader`/`io.Writer` 是理解一大批标准库/第三方库 API 的钥匙；HTTP 服务先用 `net/http` 撑起最小实现，确有需要再引入框架。

---

# 回到总览

本文档覆盖了 [core_concepts.md](./core_concepts.md) 优先级表格中 ⭐⭐⭐⭐☆ 的全部 4 组知识点。接下来可以继续阅读：

- [priority-p0-core.md](./priority-p0-core.md) —— ⭐⭐⭐⭐⭐ 核心数据模型与工程组织
- [priority-p2-runtime.md](./priority-p2-runtime.md) —— ⭐⭐⭐☆☆ Runtime、GC
- [priority-p3-advanced.md](./priority-p3-advanced.md) —— ⭐⭐☆☆☆ reflect、unsafe、cgo
