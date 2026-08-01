# Go 核心知识点代码示例 —— ⭐⭐⭐☆☆ Runtime、GC

> 本文档拆分自 [core_concepts.md](./core_concepts.md) 中"80/20 学习优先级"表格的第三梯队。
>
> 这一梯队用于**性能调优与理解底层**：Go Runtime 相当于内嵌在程序里的一个小型操作系统，负责调度 goroutine、管理内存和垃圾回收。日常业务开发不需要天天接触，但排查性能问题、内存问题时是必备知识。

---

# 1. Runtime、GC

## 核心思想

```
Go Runtime = 一个内嵌在程序里的小型 OS

负责：
  调度器（Scheduler）— 把 goroutine 映射到 OS 线程上执行
  垃圾回收（GC）     — 自动管理内存，程序员不需要手动 malloc/free
  栈管理             — goroutine 的栈会自动增长/收缩
```

程序员不需要手动管理线程创建和内存释放，但要理解 runtime 的行为，才能写出低延迟、高吞吐的 Go 服务。

## 为什么重要

- `GOMAXPROCS` 决定并行度，直接影响 CPU 密集型程序的性能
- 理解 GC 的触发时机和 STW（stop-the-world）行为，有助于写出低延迟的服务
- `pprof` 是 Go 生态标准的性能分析工具，几乎所有生产服务都应该接入
- 内存逃逸分析帮助理解"为什么这段代码比预期慢/分配多"

## 代码示例

### 示例 1：runtime.GOMAXPROCS——控制并行度

```go
package main

import (
	"fmt"
	"runtime"
)

func main() {
	fmt.Println("NumCPU:", runtime.NumCPU())
	fmt.Println("GOMAXPROCS (before):", runtime.GOMAXPROCS(0))

	runtime.GOMAXPROCS(2) // 限制最多2个OS线程并行执行goroutine
	fmt.Println("GOMAXPROCS (after):", runtime.GOMAXPROCS(0))
}
```

### 示例 2：runtime.NumGoroutine——观察 goroutine 数量（排查泄漏）

```go
package main

import (
	"fmt"
	"runtime"
	"time"
)

func leakyWorker(ch chan int) {
	<-ch // 如果没人发送数据，这个goroutine会永远阻塞在这里，造成泄漏
}

func main() {
	fmt.Println("before:", runtime.NumGoroutine())

	ch := make(chan int)
	for i := 0; i < 5; i++ {
		go leakyWorker(ch)
	}

	time.Sleep(100 * time.Millisecond)
	fmt.Println("after:", runtime.NumGoroutine()) // 多了5个泄漏的goroutine
}
```

### 示例 3：runtime.ReadMemStats——查看内存与 GC 统计

```go
package main

import (
	"fmt"
	"runtime"
)

func main() {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	fmt.Printf("Alloc = %v KB\n", m.Alloc/1024)
	fmt.Printf("TotalAlloc = %v KB\n", m.TotalAlloc/1024)
	fmt.Printf("NumGC = %v\n", m.NumGC)
}
```

### 示例 4：手动触发 GC + 查看最近一次 GC 暂停时间

```go
package main

import (
	"fmt"
	"runtime"
	"time"
)

func main() {
	start := time.Now()
	runtime.GC() // 手动触发一次同步的垃圾回收（生产代码一般不需要这样做）
	fmt.Println("GC took:", time.Since(start))

	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	fmt.Println("last GC pause (ns):", m.PauseNs[(m.NumGC+255)%256])
}
```

### 示例 5：内存逃逸分析——go build -gcflags="-m"

```go
package main

func makeOnStack() int {
	x := 42 // 没有被外部引用，通常分配在栈上
	return x
}

func makeOnHeap() *int {
	x := 42
	return &x // 返回了局部变量的地址，编译器会让它"逃逸"到堆上
}

func main() {
	_ = makeOnStack()
	_ = makeOnHeap()
}
```

```bash
# 查看逃逸分析结果
go build -gcflags="-m" main.go
# 输出中会看到类似：
# ./main.go:9:2: moved to heap: x
```

### 示例 6：net/http/pprof——接入性能分析

```go
package main

import (
	"fmt"
	"net/http"
	_ "net/http/pprof" // 匿名导入，自动注册 /debug/pprof/ 路由
)

func main() {
	fmt.Println("pprof available at http://localhost:6060/debug/pprof/")
	http.ListenAndServe("localhost:6060", nil)
}
```

```bash
# 采集30秒的CPU profile并进入交互式分析
go tool pprof http://localhost:6060/debug/pprof/profile?seconds=30
```

## 快速上手小结

日常开发不需要主动调用 `runtime.GC()`；怀疑有性能/内存问题时，第一步接 `net/http/pprof`，第二步看 `go build -gcflags="-m"` 的逃逸分析，再决定要不要调整数据结构或 `GOMAXPROCS`。

---

# 回到总览

本文档覆盖了 [core_concepts.md](./core_concepts.md) 优先级表格中 ⭐⭐⭐☆☆ 的知识点。接下来可以继续阅读：

- [priority-p0-core.md](./priority-p0-core.md) —— ⭐⭐⭐⭐⭐ 核心数据模型与工程组织
- [priority-p1-concurrency.md](./priority-p1-concurrency.md) —— ⭐⭐⭐⭐☆ 并发与标准库
- [priority-p3-advanced.md](./priority-p3-advanced.md) —— ⭐⭐☆☆☆ reflect、unsafe、cgo
