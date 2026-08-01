# Channel 最佳实践与典型示例

> **前置阅读：** [make.md](make.md) — 理解 hchan descriptor 与 `make(chan T, ...)` 的 runtime 含义
>
> **一句话心智模型：**
>
> **Channel = 类型化管道 + 同步点。`make(chan T)` 同步交接；`make(chan T, n)` 解耦生产者与消费者。**

**相关文档：** [Slice 最佳实践](slice-practices.md) · [Map 最佳实践](map-practices.md)

---

## 核心心智模型

```
无缓冲 make(chan T)              有缓冲 make(chan T, n)
                                 +---+---+---+---+
Goroutine A                      | x | x |   |   |  ring buffer
    |                            +---+---+---+---+
    |  send                      cap=n, len=当前元素数
    v
 [ rendezvous ]  <--- 必须双方同时到达
    |
    v
Goroutine B
    recv
```

**五条原则：**

1. **无缓冲 channel** = rendezvous（同步交接）：send 阻塞直到 recv，反之亦然。
2. **有缓冲 channel** = 异步队列：buffer 未满时 send 不阻塞，未满时 recv 不阻塞。
3. **nil channel** 上 send/recv **永久阻塞**；在 `select` 中 nil case 永不选中（可用来动态禁用分支）。
4. **关闭 channel** 由 sender 负责；close 后 send panic，recv 返回零值且 `ok==false`。
5. **"Don't communicate by sharing memory; share memory by communicating."** — channel 传递所有权，而非共享变量。

---

## 何时用 make / 何时不用

| 场景 | 推荐写法 | 原因 |
|------|----------|------|
| 同步 handoff / request-response | `make(chan T)` | 保证交付后才返回 |
| 生产者-消费者 / 任务队列 | `make(chan T, n)` | 解耦速率，n 为背压上限 |
| 信号通知（无数据） | `make(chan struct{})` | 零字节 struct{} |
| 限并发（信号量） | `make(chan struct{}, n)` | n 个 token |
| 仅作类型占位、不通信 | 不要用 channel | 用 struct 字段或参数 |

**buffer 大小选择：** 不要盲目设很大；buffer 应反映可接受的积压量，过大掩盖背压问题。

---

## 10+ 典型示例

### 示例 1：无缓冲同步 handoff

**场景：** request-response，确保数据被接收后才继续。

```go
package main

import "fmt"

func worker(req chan int, resp chan int) {
	n := <-req
	resp <- n * 2
}

func main() {
	req := make(chan int)
	resp := make(chan int)

	go worker(req, resp)

	req <- 21
	result := <-resp
	fmt.Println(result) // 42
}
```

**最佳实践：** 无缓冲 channel 保证**同步交付**——send 返回时 receiver 已拿到值。适合 handoff、确认、锁步协调。

---

### 示例 2：有缓冲队列

**场景：** 生产者快于消费者，允许有限积压。

```go
package main

import "fmt"

func main() {
	queue := make(chan int, 3) // buffer=3

	queue <- 1
	queue <- 2
	queue <- 3
	// queue <- 4 // 若无 receiver，此处会阻塞

	fmt.Println(<-queue) // 1
	fmt.Println(<-queue) // 2
	fmt.Println(<-queue) // 3
}
```

**最佳实践：** buffer 满时 send 阻塞，空时 recv 阻塞——这是天然的**背压**机制。

---

### 示例 3：`close` + `range` 通知结束

**场景：** 生产者完成后通知消费者退出循环。

```go
package main

import "fmt"

func producer(out chan<- int) {
	for i := 1; i <= 3; i++ {
		out <- i
	}
	close(out) // 通知：不会再有数据
}

func main() {
	ch := make(chan int)
	go producer(ch)

	for v := range ch {
		fmt.Println(v) // 1, 2, 3
	}
	fmt.Println("done")
}
```

**最佳实践：** `close(ch)` 后 `range ch` 自动退出；**只有 sender 应该 close**，receiver 不要 close。

---

### 示例 4：`select` 多路复用

**场景：** 同时等待多个 channel。

```go
package main

import (
	"fmt"
	"time"
)

func main() {
	ch1 := make(chan string)
	ch2 := make(chan string)

	go func() {
		time.Sleep(50 * time.Millisecond)
		ch1 <- "from ch1"
	}()
	go func() {
		time.Sleep(10 * time.Millisecond)
		ch2 <- "from ch2"
	}()

	for i := 0; i < 2; i++ {
		select {
		case msg := <-ch1:
			fmt.Println(msg)
		case msg := <-ch2:
			fmt.Println(msg)
		}
	}
}
```

**最佳实践：** `select` 随机选择一个就绪 case；多个 case 同时就绪时行为非确定性（公平随机）。

---

### 示例 5：done / cancel 信号

**场景：** 通知 goroutine 停止工作。

```go
package main

import (
	"fmt"
	"time"
)

func worker(done <-chan struct{}) {
	for {
		select {
		case <-done:
			fmt.Println("worker stopped")
			return
		default:
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func main() {
	done := make(chan struct{})
	go worker(done)

	time.Sleep(30 * time.Millisecond)
	close(done) // 广播：所有 <-done 的 goroutine 都会收到
	time.Sleep(20 * time.Millisecond)
}
```

**最佳实践：** `chan struct{}` 是零成本的信号 channel；`close(done)` 可通知多个 goroutine。生产环境优先用 `context.Context`。

---

### 示例 6：nil channel 在 select 中

**场景：** 动态禁用 select 的某个分支。

```go
package main

import "fmt"

func main() {
	ch := make(chan int, 1)
	ch <- 42

	var disabled chan int // nil channel

	select {
	case v := <-ch:
		fmt.Println("got:", v) // got: 42
	case v := <-disabled:
		fmt.Println("never:", v) // 此分支永不执行
	}
}
```

**最佳实践：** nil channel 在 select 中**永远不会被选中**，可用于条件性地关闭某个 case（例如 merge 多个 channel 时）。

---

### 示例 7：关闭后 send panic

**场景：** 理解 close 的语义边界。

```go
package main

import "fmt"

func main() {
	ch := make(chan int, 1)
	ch <- 1
	close(ch)

	v, ok := <-ch
	fmt.Println(v, ok) // 1 true

	v, ok = <-ch
	fmt.Println(v, ok) // 0 false — channel 已 drained

	// ch <- 2 // panic: send on closed channel
}
```

**最佳实践：** close 表示"不会再 send"；向 closed channel send 会 panic。用 `sync.Once` 保证只 close 一次。

---

### 示例 8：关闭后 recv

**场景：** 从 closed channel 读取的行为。

```go
package main

import "fmt"

func main() {
	ch := make(chan int, 2)
	ch <- 10
	ch <- 20
	close(ch)

	for {
		v, ok := <-ch
		if !ok {
			fmt.Println("channel closed")
			break
		}
		fmt.Println(v)
	}
	// 10, 20, channel closed
}
```

**最佳实践：** drained 后继续 recv 返回 `(zeroValue, false)`，不会 panic。`range` 等价于此循环。

---

### 示例 9：fan-out / fan-in

**场景：** 多 worker 并行处理，结果合并。

```go
package main

import (
	"fmt"
	"sync"
)

func fanOut(in <-chan int, n int) []<-chan int {
	outs := make([]<-chan int, n)
	for i := 0; i < n; i++ {
		out := make(chan int)
		outs[i] = out
		go func(ch chan<- int) {
			defer close(ch)
			for v := range in {
				ch <- v * 2
			}
		}(out)
	}
	return outs
}

func fanIn(chs ...<-chan int) <-chan int {
	out := make(chan int)
	var wg sync.WaitGroup
	wg.Add(len(chs))
	for _, ch := range chs {
		go func(c <-chan int) {
			defer wg.Done()
			for v := range c {
				out <- v
			}
		}(ch)
	}
	go func() {
		wg.Wait()
		close(out)
	}()
	return out
}

func main() {
	in := make(chan int)
	go func() {
		for i := 1; i <= 3; i++ {
			in <- i
		}
		close(in)
	}()

	outs := fanOut(in, 2)
	result := fanIn(outs...)
	for v := range result {
		fmt.Println(v) // 2 4 6（顺序不定）
	}
}
```

**最佳实践：** fan-out 分发任务，fan-in 合并结果；注意关闭链条，避免 goroutine 泄漏。

---

### 示例 10：pipeline 链式处理

**场景：** 多 stage 通过 channel 串联。

```go
package main

import "fmt"

func gen(nums ...int) <-chan int {
	out := make(chan int)
	go func() {
		for _, n := range nums {
			out <- n
		}
		close(out)
	}()
	return out
}

func square(in <-chan int) <-chan int {
	out := make(chan int)
	go func() {
		for n := range in {
			out <- n * n
		}
		close(out)
	}()
	return out
}

func main() {
	for v := range square(gen(1, 2, 3, 4)) {
		fmt.Println(v) // 1 4 9 16
	}
}
```

**最佳实践：** 每个 stage 是 `func(<-chan T) <-chan U`；用 directional channel（`<-chan` / `chan<-`）表达读写权限。

---

### 示例 11：信号量（限并发）

**场景：** 最多 N 个 goroutine 同时执行。

```go
package main

import (
	"fmt"
	"sync"
	"time"
)

func main() {
	const maxConcurrent = 2
	sem := make(chan struct{}, maxConcurrent)
	var wg sync.WaitGroup

	for i := 1; i <= 5; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			sem <- struct{}{}        // acquire
			defer func() { <-sem }() // release
			fmt.Println("worker", id)
			time.Sleep(50 * time.Millisecond)
		}(i)
	}
	wg.Wait()
}
```

**最佳实践：** `make(chan struct{}, n)` 作为 counting semaphore；acquire 发送，release 接收。

---

### 示例 12：timeout / 非阻塞

**场景：** 避免永久阻塞在 channel 操作上。

```go
package main

import (
	"fmt"
	"time"
)

func main() {
	ch := make(chan int)

	// timeout
	select {
	case v := <-ch:
		fmt.Println(v)
	case <-time.After(50 * time.Millisecond):
		fmt.Println("timeout")
	}

	// 非阻塞（default）
	select {
	case v := <-ch:
		fmt.Println(v)
	default:
		fmt.Println("no data ready")
	}
}
```

**最佳实践：** `time.After` 用于超时；`default` 用于非阻塞尝试。注意 `time.After` 在循环中会泄漏 timer，热路径用 `time.NewTimer`。

---

### 示例 13：避免 goroutine 泄漏

**场景：** 确保每个 blocked goroutine 有出路。

```go
package main

import (
	"context"
	"fmt"
	"time"
)

func leakyWorker(ctx context.Context, out chan<- int) {
	for i := 0; ; i++ {
		select {
		case <-ctx.Done():
			return // 有 cancel 出口，不会泄漏
		case out <- i:
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	out := make(chan int, 1)

	go leakyWorker(ctx, out)

	for i := 0; i < 3; i++ {
		fmt.Println(<-out)
	}
	cancel() // 停止 worker
	time.Sleep(20 * time.Millisecond)
}
```

**最佳实践：** 每个 goroutine 都应有退出路径（context cancel、done channel、close input）。无 receiver 的无缓冲 send 会永久阻塞。

**常见泄漏场景：**

- 无缓冲 channel send 但没有 receiver
- `range` 永远不会 close 的 channel
- buffer 满且没有 consumer

---

## 反模式速查表

| 反模式 | 问题 | 正确做法 |
|--------|------|----------|
| 超大 buffer | 掩盖背压，内存积压 | 按业务容忍度设合理 n |
| 所有共享状态都用 channel | 过度复杂 | 简单计数器用 `Mutex` |
| receiver close channel | 语义混乱，send panic | sender close |
| close 后仍 send | panic | 用 done 信号或 context |
| 无退出路径的 goroutine | 泄漏 | context / done / close input |
| 循环中 `time.After` | timer 泄漏 | `time.NewTimer` + Stop |
| nil channel 上直接 send/recv | 永久阻塞 | 必须 make 后再用 |

---

## 与 make.md 的关联

| 主题 | make.md 章节 |
|------|-------------|
| `make(chan T)` 无缓冲 | [第九节 Channel 的 make](make.md#九channel-的-make) |
| `make(chan T, n)` 有缓冲 | [第九节 有缓冲](make.md#九channel-的-make) |
| nil channel 永久阻塞 | [第十一节 make 与 nil 的区别](make.md#十一make-与-nil-的区别) |
| 按业务选 buffer | [第十三节最佳实践 #3](make.md#十三make-的最佳实践) |

---

## 快速上手小结

> **无缓冲 = 同步交接；有缓冲 = 解耦队列。sender 负责 close。每个 goroutine 都要有退出路径。channel 传数据所有权，不要用它替代所有锁——简单共享状态用 Mutex 更清晰。**
