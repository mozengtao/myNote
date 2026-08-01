# Go 80/20 学习路线 —— 覆盖 80% Go 开发的 20% 核心知识

> **目标**
>
> 用最少的知识覆盖绝大多数 Go 项目（Web、CLI、Backend、Cloud Native、Kubernetes、Docker、网络服务等）。
>
> **一句话心智模式：**
>
> > **Go = 简单的数据 + 明确的接口 + CSP 并发 + 工程化工具链。**

---

# Go 的整体心智模式

```
          Data
            │
      struct + builtin types
            │
            ▼
        Functions
            │
            ▼
        Interface
            │
            ▼
      Package Organization
            │
            ▼
      Goroutine + Channel
            │
            ▼
      Context + Error
            │
            ▼
     Production Engineering
```

学习 Go，不要从语法开始。

而应该从这几个问题开始：

> 数据是什么？

↓

> 谁处理数据？

↓

> 如何抽象能力？

↓

> 如何并发？

↓

> 如何组织工程？

---

# 1. 一切围绕 Value（值）

## Go 的第一原则

Go 不是面向对象语言。

Go 更像：

> **Everything is Value**

几乎所有东西都是值。

```
int

string

struct

array

function

channel

interface

pointer
```

都是值。

例如：

```go
a := 10

b := a

b++

fmt.Println(a)
```

输出

```
10
```

因为：

```
a ----10

b ----10
```

复制的是值。

---

## 心智模式

```
Go
↓

Value Copy Language
```

永远先问：

> 我现在复制的是值？

还是引用？

---

# 2. struct 是数据，不是对象

Go 的 struct 非常简单。

```go
type User struct {
    Name string
    Age  int
}
```

它就是：

```
一块连续内存
```

没有：

- 继承
- 虚函数
- constructor
- property

只有数据。

---

## 方法只是函数

例如

```go
func (u User) Print() {}
```

其实就是：

```go
Print(User)
```

receiver 只是第一个参数。

---

## 心智模式

```
struct

=

Data
```

不是对象。

---

# 3. Method 是语法糖

例如

```go
u.Print()
```

其实就是

```
Print(u)
```

pointer receiver

```go
u.Update()
```

其实就是

```
Update(&u)
```

Go 没有真正意义上的成员函数。

---

## 心智模式

```
Method

=

Function
+
Receiver
```

---

# 4. Interface 是行为，不是类型

这是 Go 最重要的思想。

例如

```go
type Reader interface {
    Read([]byte)(int,error)
}
```

不是说：

> Reader 是某种对象

而是：

> Reader 会 Read。

---

例如：

```
File

Socket

Buffer

HTTP Body
```

全部都能：

```
Read()
```

所以：

```
都是 Reader
```

---

## Duck Typing

Go 不需要：

```
implements Reader
```

只要：

```
有 Read()
```

即可。

---

## 心智模式

```
Interface

=

Capability（能力）
```

不是父类。

---

# 5. Package 是组织单位

Go 没有 namespace。

没有 class。

Package 就是一切。

```
package http

package json

package fmt
```

所有可见性：

```
首字母大小写
```

决定。

---

## 心智模式

```
Package

=

Module
```

不是 Folder。

---

# 6. Error 是普通返回值

Go 不用 Exception。

例如：

```go
v, err := os.Open(name)
```

错误：

就是：

```
普通值
```

因此：

```
if err != nil
```

成为 Go 最经典代码。

---

## 心智模式

```
Error

=

Expected Result
```

不是异常。

---

# 7. defer = 离开作用域时执行

例如：

```go
f,_:=os.Open()

defer f.Close()
```

无论：

```
return

panic

多个 return
```

都会执行。

---

## 心智模式

```
Acquire

↓

Defer Release
```

类似：

C++ RAII

但是：

Go 是函数退出执行。

---

# 8. Pointer 只是共享数据

Go 指针非常弱。

不能：

```
pointer arithmetic
```

没有：

```
++
--
```

没有：

```
*(p+1)
```

只有：

```
共享数据
```

例如：

```go
func Update(u *User)
```

目的：

不是提高效率。

而是：

```
修改原对象
```

---

## 心智模式

```
Pointer

=

Shared Data
```

不是地址计算。

---

# 9. Slice 是最重要的数据结构

Go 最重要：

不是 array。

而是：

```
slice
```

例如：

```go
nums:=[]int{1,2,3}
```

底层：

```
Pointer

Length

Capacity
```

```
Slice

↓

Array
```

---

append()

可能：

```
扩容

复制

重新指向数组
```

所以：

```
slice

不是数组。
```

---

## 心智模式

```
Slice

=

Window of Array
```

不是动态数组。

---

# 10. Map 是 Hash Table

```
map[K]V
```

就是：

HashMap。

特点：

```
引用语义

自动扩容

无序
```

---

## 心智模式

```
Map

=

Hash Table
```

---

# 11. Goroutine = 极轻量线程

启动：

```go
go Worker()
```

即可。

不是：

```
OS Thread
```

而是：

```
Goroutine
```

Go Runtime：

负责：

```
调度

切换

抢占
```

---

## 心智模式

```
Go Routine

=

Task
```

不是线程。

---

# 12. Channel = 通信，而不是共享内存

Go 最经典思想：

```
Don't communicate by sharing memory.

Share memory by communicating.
```

例如：

```
Producer

↓

Channel

↓

Consumer
```

---

不要：

```
mutex

↓

共享变量
```

而是：

```
goroutine

↓

channel

↓

goroutine
```

---

## 心智模式

```
Channel

=

Pipe
```

数据流。

不是 Queue。

---

# 13. Context = 生命周期

现代 Go 最重要组件。

里面只有：

```
Deadline

Cancel

Value
```

例如：

HTTP 请求：

```
Client

↓

Request

↓

DB

↓

RPC
```

共享：

```
Context
```

取消一次：

全部停止。

---

## 心智模式

```
Context

=

Cancellation Tree
```

不是配置对象。

---

# 14. Goroutine + Channel = CSP

Go 并发不是：

```
共享变量
```

而是：

```
Worker

↓

Channel

↓

Worker
```

经典：

```
Pipeline

Worker Pool

Fan-Out

Fan-In
```

---

## 心智模式

```
Concurrency

=

Message Passing
```

---

# 15. Go Runtime 管理一切

Go Runtime：

负责：

```
GC

Scheduler

Stack

Panic

Recover

Timer

Channel
```

程序员：

不用：

```
malloc

free

thread create
```

---

## 心智模式

```
Go Runtime

=

Small OS
```

---

# 16. 标准库优先（Standard Library First）

Go 的一个设计哲学：

> **优先使用标准库，而不是第三方框架。**

掌握以下几个包，就能完成大多数开发：

| 包 | 用途 |
|-----|------|
| `fmt` | 格式化输出 |
| `os` | 文件、环境变量、进程 |
| `io` | I/O 抽象 |
| `bufio` | 缓冲 I/O |
| `strings` | 字符串处理 |
| `bytes` | 字节缓冲 |
| `strconv` | 类型转换 |
| `time` | 时间处理 |
| `context` | 生命周期控制 |
| `sync` | 并发同步 |
| `net/http` | HTTP 服务 |
| `encoding/json` | JSON |

---

## 心智模式

```
Standard Library

>

Framework
```

---

# 17. Go Modules = 依赖管理

现代 Go 使用：

```
go.mod

go.sum
```

管理依赖。

常用命令：

```bash
go mod init
go mod tidy
go get
```

---

## 心智模式

```
Project

↓

Module

↓

Package
```

---

# 18. 工具链即生产力

Go 的工具链几乎是语言的一部分：

```bash
go build
go run
go test
go fmt
go vet
go test -race
go mod tidy
```

多数团队会把 `go fmt` 和 `go test` 作为提交前的默认流程。

---

## 心智模式

```
Code

↓

Toolchain

↓

Executable
```

---

# 19. Go 的工程组织

一个典型项目通常包括：

```
cmd/
internal/
pkg/
api/
configs/
scripts/
```

依赖方向：

```
main
 │
 ▼
service
 │
 ▼
repository
 │
 ▼
database
```

保持依赖单向流动，避免循环依赖。

---

## 心智模式

```
Project

↓

Packages

↓

Dependencies
```

---

# 20. Go 的整体思维模型（最重要）

如果只能记住一张图，请记住下面这一张。

```
                     Go

                     │
        ┌────────────┼────────────┐
        │            │            │
        ▼            ▼            ▼
      Value       Function     Package
        │            │            │
        └────────────┬────────────┘
                     ▼
                 Interface
            (Behavior / Capability)
                     │
                     ▼
              Goroutine + Channel
               (Message Passing)
                     │
                     ▼
              Context + Error
         (Lifecycle & Failure)
                     │
                     ▼
                Runtime + GC
                     │
                     ▼
            Production Engineering
```

---

# 80/20 学习优先级

| 优先级 | 知识点 | 为什么重要 |
|---------|--------|------------|
| ⭐⭐⭐⭐⭐ | Value、Pointer、Struct | 理解 Go 的数据模型 |
| ⭐⭐⭐⭐⭐ | Function、Method | 理解 Go 的组织方式 |
| ⭐⭐⭐⭐⭐ | Interface | Go 抽象能力的核心 |
| ⭐⭐⭐⭐⭐ | Slice、Map | 覆盖绝大多数数据处理 |
| ⭐⭐⭐⭐⭐ | Error、defer | Go 的资源管理与错误处理 |
| ⭐⭐⭐⭐⭐ | Package、Module | 工程组织基础 |
| ⭐⭐⭐⭐☆ | Goroutine、Channel | Go 并发编程核心 |
| ⭐⭐⭐⭐☆ | Context | 现代 Go 服务必备 |
| ⭐⭐⭐⭐☆ | sync、Mutex、WaitGroup | 并发同步基础 |
| ⭐⭐⭐⭐☆ | 标准库（io、os、net/http、json） | 覆盖绝大多数开发场景 |
| ⭐⭐⭐☆☆ | Runtime、GC | 性能调优与理解底层 |
| ⭐⭐☆☆☆ | reflect、unsafe、cgo | 特殊场景使用，按需学习 |

每个优先级都配有独立的代码示例文档（每个知识点至少 5 个典型示例，帮助快速上手）：

- [priority-p0-core.md](./priority-p0-core.md) —— ⭐⭐⭐⭐⭐ Value/Pointer/Struct、Function/Method、Interface、Slice/Map、Error/defer、Package/Module
- [priority-p1-concurrency.md](./priority-p1-concurrency.md) —— ⭐⭐⭐⭐☆ Goroutine/Channel、Context、sync/Mutex/WaitGroup、标准库
- [priority-p2-runtime.md](./priority-p2-runtime.md) —— ⭐⭐⭐☆☆ Runtime、GC
- [priority-p3-advanced.md](./priority-p3-advanced.md) —— ⭐⭐☆☆☆ reflect、unsafe、cgo

---

# 一句话总结 Go 的核心心智模式

> **Go 是一门以 Value 为基础、以 Interface 抽象行为、以 Package 组织代码、以 Goroutine + Channel 实现 CSP 并发、以 Context 管理生命周期、依赖标准库和工具链构建工程化软件的语言。掌握这些核心概念，足以覆盖绝大多数 Go 开发场景。**