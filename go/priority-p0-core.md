# Go 核心知识点代码示例 —— ⭐⭐⭐⭐⭐ 最高优先级

> 本文档拆分自 [core_concepts.md](./core_concepts.md) 中"80/20 学习优先级"表格的第一梯队。
>
> 这一梯队是 Go 语言的地基：**理解数据模型（Value / Pointer / Struct）、组织方式（Function / Method）、抽象能力（Interface）、数据处理（Slice / Map）、资源管理（Error / defer）、工程组织（Package / Module）**，就能读懂并写出绝大多数 Go 代码。

---

# 1. Value、Pointer、Struct

## 核心思想

```
struct  = 一块连续内存（纯数据，不是对象）
value   = 复制一份独立的数据
pointer = 共享同一份数据
```

Go 里赋值、传参、返回值，默认都是**值拷贝**。要不要共享数据，取决于你传的是值还是指针，这是一个必须显式做出的选择。

## 为什么重要

- 决定"修改会不会影响外面"，是 Go 里最容易踩坑、也最需要肌肉记忆的规则
- 是理解方法接收者选择（value receiver vs pointer receiver）的前提
- 是理解 slice / map / interface 底层行为的基础（它们内部也是值拷贝 + 共享数据的组合）
- 直接影响性能：大 struct 传值 vs 传指针，开销差异很大

## 代码示例

### 示例 1：值拷贝——修改副本不影响原始数据

```go
package main

import "fmt"

type Point struct {
	X, Y int
}

func main() {
	p1 := Point{X: 1, Y: 2}
	p2 := p1 // 值拷贝，p2 是一份独立的数据
	p2.X = 100

	fmt.Println(p1) // {1 2}
	fmt.Println(p2) // {100 2}
}
```

### 示例 2：指针共享——通过指针修改会影响原始数据

```go
package main

import "fmt"

type Point struct {
	X, Y int
}

func moveByValue(p Point) {
	p.X += 10 // 修改的是副本，不影响外部
}

func moveByPointer(p *Point) {
	p.X += 10 // 通过指针修改，影响原始数据
}

func main() {
	p := Point{X: 1, Y: 2}

	moveByValue(p)
	fmt.Println(p) // {1 2} 没变

	moveByPointer(&p)
	fmt.Println(p) // {11 2} 变了
}
```

### 示例 3：方法接收者选择——value receiver vs pointer receiver

```go
package main

import "fmt"

type Counter struct {
	count int
}

// value receiver：方法内部操作的是 Counter 的副本
func (c Counter) IncByValue() {
	c.count++
}

// pointer receiver：方法内部操作的是原始 Counter
func (c *Counter) IncByPointer() {
	c.count++
}

func main() {
	c := Counter{}

	c.IncByValue()
	fmt.Println(c.count) // 0，没变

	c.IncByPointer()
	fmt.Println(c.count) // 1，变了
}
```

### 示例 4：struct 的零值与可比较性

```go
package main

import "fmt"

type Config struct {
	Host string
	Port int
}

func main() {
	var c Config // 零值：Host="" Port=0
	fmt.Println(c)

	c1 := Config{Host: "a", Port: 80}
	c2 := Config{Host: "a", Port: 80}
	// 字段都是可比较类型时，struct 之间可以直接用 == 比较
	fmt.Println(c1 == c2) // true
}
```

### 示例 5：常见陷阱——for-range 复用循环变量

```go
package main

import "fmt"

type Item struct {
	Name string
}

func main() {
	items := []Item{{"a"}, {"b"}, {"c"}}

	var ptrs []*Item
	for _, item := range items {
		item := item // 显式拷贝一份，避免多个指针指向同一个循环变量
		ptrs = append(ptrs, &item)
	}

	for _, p := range ptrs {
		fmt.Println(p.Name) // a b c
	}
	// 注意：Go 1.22 之后 for-range 的循环变量语义已修正为每次迭代都是新变量，
	// 但在阅读旧代码、维护旧项目时仍需要理解这个坑。
}
```

### 示例 6：大 struct 传值的开销——什么时候该用指针

```go
package main

import "fmt"

type BigStruct struct {
	Data [1024]byte
	Name string
}

// 传值：整个 BigStruct（1000+ 字节）都会被拷贝，开销大
func processByValue(b BigStruct) {
	_ = b
}

// 传指针：只拷贝一个指针（8 字节），开销小
func processByPointer(b *BigStruct) {
	_ = b
}

func main() {
	b := BigStruct{Name: "big"}
	processByValue(b)
	processByPointer(&b)
	fmt.Println("done")
}
```

## 快速上手小结

看到"要不要影响外部数据"就想到指针；看到"结构体很大、频繁传递"就默认用指针；其余场景优先用值，值语义更安全、更少 bug。

---

# 2. Function、Method

## 核心思想

```
Method = Function + Receiver（语法糖）
```

函数在 Go 里是一等值：可以赋值给变量、作为参数传递、作为返回值，也可以形成闭包。Method 只是给函数加了一个"隐式的第一个参数"（receiver）。

## 为什么重要

- 函数是一等值，是回调、中间件、依赖注入等设计模式的基石
- 理解 method = function + receiver，才能理解方法集、接口实现的底层原理
- 多返回值 + 具名返回值是 Go 错误处理和 API 设计的基础
- 可变参数、闭包被标准库（`fmt`、`sort`、`http` 中间件）大量使用

## 代码示例

### 示例 1：多返回值与具名返回值

```go
package main

import "fmt"

func divide(a, b int) (result int, err error) {
	if b == 0 {
		err = fmt.Errorf("division by zero")
		return // 具名返回值，直接 return 即可
	}
	result = a / b
	return
}

func main() {
	r, err := divide(10, 2)
	fmt.Println(r, err) // 5 <nil>

	r, err = divide(10, 0)
	fmt.Println(r, err) // 0 division by zero
}
```

### 示例 2：函数是一等值——作为参数传递（回调）

```go
package main

import "fmt"

func apply(nums []int, f func(int) int) []int {
	result := make([]int, len(nums))
	for i, n := range nums {
		result[i] = f(n)
	}
	return result
}

func main() {
	nums := []int{1, 2, 3}
	doubled := apply(nums, func(n int) int { return n * 2 })
	fmt.Println(doubled) // [2 4 6]
}
```

### 示例 3：闭包——函数捕获外部变量

```go
package main

import "fmt"

func makeCounter() func() int {
	count := 0
	return func() int {
		count++
		return count
	}
}

func main() {
	counter := makeCounter()
	fmt.Println(counter()) // 1
	fmt.Println(counter()) // 2
	fmt.Println(counter()) // 3
}
```

### 示例 4：Method 的本质——等价于普通函数

```go
package main

import "fmt"

type Rectangle struct {
	Width, Height float64
}

func (r Rectangle) Area() float64 {
	return r.Width * r.Height
}

// 等价的函数写法：receiver 只是第一个参数
func Area(r Rectangle) float64 {
	return r.Width * r.Height
}

func main() {
	r := Rectangle{Width: 3, Height: 4}
	fmt.Println(r.Area()) // 12
	fmt.Println(Area(r))  // 12，两者完全等价
}
```

### 示例 5：可变参数（variadic）

```go
package main

import "fmt"

func sum(nums ...int) int {
	total := 0
	for _, n := range nums {
		total += n
	}
	return total
}

func main() {
	fmt.Println(sum(1, 2, 3)) // 6

	nums := []int{4, 5, 6}
	fmt.Println(sum(nums...)) // 15，用 ... 展开 slice 传入
}
```

### 示例 6：方法集陷阱——pointer receiver 不在 value 的方法集里

```go
package main

import "fmt"

type Speaker interface {
	Speak() string
}

type Dog struct{}

// pointer receiver
func (d *Dog) Speak() string { return "Woof" }

func main() {
	var s Speaker
	d := Dog{}
	// s = d // 编译错误：Dog 的方法集不包含 Speak（因为是 pointer receiver）
	s = &d // 必须取地址，*Dog 的方法集才包含 Speak
	fmt.Println(s.Speak())
}
```

## 快速上手小结

写方法时先问"要不要修改接收者、接收者是否很大"；只要有一个方法用了 pointer receiver，通常整个类型的所有方法都统一用 pointer receiver，保持方法集一致。

---

# 3. Interface

## 核心思想

```
Interface = 能力（Capability），不是父类
```

Go 的接口是**隐式实现**的：只要类型拥有接口要求的方法，就自动满足接口，不需要 `implements` 声明。这是 Go 实现松耦合的核心机制。

## 为什么重要

- 是 Go 实现依赖倒置、解耦、mock 测试的核心手段
- `io.Reader` / `io.Writer` 等标准库大量基于"小接口 + 组合"
- `error` 本身就是一个接口，理解它才能理解 Go 的错误处理体系
- `any` + 类型断言 / 类型开关，是处理"任意类型"的标准方式

## 代码示例

### 示例 1：定义与隐式实现

```go
package main

import "fmt"

type Speaker interface {
	Speak() string
}

type Cat struct{}

func (c Cat) Speak() string { return "Meow" }

type Robot struct{}

func (r Robot) Speak() string { return "Beep" }

func announce(s Speaker) {
	fmt.Println(s.Speak())
}

func main() {
	announce(Cat{})   // Meow
	announce(Robot{}) // Beep，Cat 和 Robot 都没有声明"implements Speaker"
}
```

### 示例 2：空接口与 any

```go
package main

import "fmt"

func describe(v any) {
	fmt.Printf("value=%v, type=%T\n", v, v)
}

func main() {
	describe(42)
	describe("hello")
	describe(3.14)
	describe([]int{1, 2, 3})
}
```

### 示例 3：类型断言

```go
package main

import "fmt"

func main() {
	var v any = "hello"

	s, ok := v.(string)
	fmt.Println(s, ok) // hello true

	n, ok := v.(int)
	fmt.Println(n, ok) // 0 false，双返回值形式断言失败不会 panic
}
```

### 示例 4：类型开关（type switch）

```go
package main

import "fmt"

func printType(v any) {
	switch x := v.(type) {
	case int:
		fmt.Println("int:", x)
	case string:
		fmt.Println("string:", x)
	case bool:
		fmt.Println("bool:", x)
	default:
		fmt.Println("unknown type:", x)
	}
}

func main() {
	printType(1)
	printType("go")
	printType(true)
	printType(3.14)
}
```

### 示例 5：error 接口与 errors.As

```go
package main

import (
	"errors"
	"fmt"
)

type NotFoundError struct {
	Resource string
}

func (e *NotFoundError) Error() string {
	return fmt.Sprintf("%s not found", e.Resource)
}

func findUser(id int) error {
	if id != 1 {
		return &NotFoundError{Resource: "user"}
	}
	return nil
}

func main() {
	err := findUser(2)

	var nf *NotFoundError
	if errors.As(err, &nf) {
		fmt.Println("resource:", nf.Resource) // resource: user
	}
}
```

### 示例 6：用小接口解耦——依赖注入与 mock 测试

```go
package main

import "fmt"

type Notifier interface {
	Notify(msg string) error
}

type EmailNotifier struct{}

func (EmailNotifier) Notify(msg string) error {
	fmt.Println("send email:", msg)
	return nil
}

type FakeNotifier struct{ Sent []string }

func (f *FakeNotifier) Notify(msg string) error {
	f.Sent = append(f.Sent, msg)
	return nil
}

type OrderService struct {
	notifier Notifier // 依赖接口，而不是具体实现
}

func (s *OrderService) PlaceOrder() {
	s.notifier.Notify("order placed")
}

func main() {
	// 生产环境用真实实现
	svc := &OrderService{notifier: EmailNotifier{}}
	svc.PlaceOrder()

	// 测试环境用 fake 实现，不需要真的发邮件
	fake := &FakeNotifier{}
	testSvc := &OrderService{notifier: fake}
	testSvc.PlaceOrder()
	fmt.Println(fake.Sent) // [order placed]
}
```

## 快速上手小结

定义接口时优先追求"小"（1-2 个方法最理想），并且让接口由**调用方**定义，而不是让实现方预先设计好一堆接口；看到"需要 mock/替换实现"的场景，就该用接口。

---

# 4. Slice、Map

## 核心思想

```
Slice = 底层数组的一个窗口（指针 + 长度 + 容量）
Map   = 哈希表，引用语义
```

Slice 不是动态数组本身，而是指向数组的一个"视图"；Map 的所有操作都直接作用于同一份底层数据。

## 为什么重要

- 覆盖 Go 中绝大多数数据处理场景（列表、集合、去重、分组、缓存）
- `append` 扩容与 slice 共享底层数组，是最容易踩坑的地方
- Map 的零值、并发不安全、遍历顺序不确定，是常见 bug 来源

## 代码示例

### 示例 1：slice 的底层结构——len 与 cap

```go
package main

import "fmt"

func main() {
	s := make([]int, 3, 5)
	fmt.Println(s, len(s), cap(s)) // [0 0 0] 3 5

	s = append(s, 1, 2)
	fmt.Println(s, len(s), cap(s)) // [0 0 0 1 2] 5 5

	s = append(s, 3) // 超过 cap，触发扩容，底层数组会被替换
	fmt.Println(s, len(s), cap(s))
}
```

### 示例 2：slice 共享底层数组的陷阱

```go
package main

import "fmt"

func main() {
	original := []int{1, 2, 3, 4, 5}
	sub := original[1:3] // sub 和 original 共享同一个底层数组

	sub[0] = 100
	fmt.Println(original) // [1 100 3 4 5]，original 也被改了！

	// 需要独立数据时，必须显式 copy
	safeCopy := make([]int, len(sub))
	copy(safeCopy, sub)
	safeCopy[0] = 999
	fmt.Println(original) // [1 100 3 4 5]，不受影响
}
```

### 示例 3：append 扩容后脱离原数组

```go
package main

import "fmt"

func main() {
	s := make([]int, 3, 3) // len == cap，没有多余容量
	s2 := append(s, 4)     // 触发扩容，s2 指向新的底层数组

	s2[0] = 999
	fmt.Println(s)  // [0 0 0]，s 没变
	fmt.Println(s2) // [999 0 0 4]，s2 是独立的新数据
}
```

### 示例 4：map 基本操作与"零值 vs 不存在"

```go
package main

import "fmt"

func main() {
	m := map[string]int{"a": 1, "b": 2}

	v := m["c"] // key 不存在，返回值类型的零值
	fmt.Println(v) // 0

	v, ok := m["c"]
	fmt.Println(v, ok) // 0 false，用 ok 才能区分"不存在"和"值恰好是0"

	m["c"] = 3
	delete(m, "a")
	fmt.Println(m) // map[b:2 c:3]
}
```

### 示例 5：map 并发不安全——需要加锁保护

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	m := make(map[int]int)
	var mu sync.Mutex
	var wg sync.WaitGroup

	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			mu.Lock() // 没有这行，并发写 map 会直接 panic: concurrent map writes
			m[n] = n * n
			mu.Unlock()
		}(i)
	}

	wg.Wait()
	fmt.Println(len(m)) // 100
}
```

### 示例 6：用 slice + map 实现去重与分组

```go
package main

import "fmt"

func dedup(nums []int) []int {
	seen := make(map[int]bool)
	result := make([]int, 0, len(nums))
	for _, n := range nums {
		if !seen[n] {
			seen[n] = true
			result = append(result, n)
		}
	}
	return result
}

func groupByParity(nums []int) map[string][]int {
	groups := make(map[string][]int)
	for _, n := range nums {
		key := "even"
		if n%2 != 0 {
			key = "odd"
		}
		groups[key] = append(groups[key], n)
	}
	return groups
}

func main() {
	nums := []int{1, 2, 2, 3, 3, 3, 4}
	fmt.Println(dedup(nums))         // [1 2 3 4]
	fmt.Println(groupByParity(nums)) // map[even:[2 2 4] odd:[1 3 3 3]]
}
```

## 快速上手小结

传递 slice/map 时默认就是"共享数据"，需要隔离修改时才要显式 `copy`；并发场景下的 map 一律加锁或用 `sync.Map`，永远不要假设它是并发安全的。

---

# 5. Error、defer

## 核心思想

```
Error = 普通返回值（不是异常）
defer = 离开作用域前必然执行的清理动作
```

Go 用"最后一个返回值是 error + `if err != nil`"取代了异常机制；`defer` 用来保证资源释放，类似 C++ 的 RAII，但是在函数退出时执行。

## 为什么重要

- `if err != nil` 是 Go 最常见的代码模式，理解它是读懂任何 Go 代码的前提
- `defer` + 资源释放（文件/锁/连接）是避免资源泄漏的标准做法
- `errors.Is` / `errors.As` / `%w` 包装是 Go 1.13+ 错误处理的标准手段
- `defer` 的参数求值时机、执行顺序是常见的面试题与 bug 来源

## 代码示例

### 示例 1：基本错误处理惯用法

```go
package main

import (
	"fmt"
	"os"
)

func main() {
	f, err := os.Open("not-exist.txt")
	if err != nil {
		fmt.Println("open failed:", err)
		return
	}
	defer f.Close()
	fmt.Println("opened:", f.Name())
}
```

### 示例 2：defer 保证资源释放

```go
package main

import "fmt"

func process() {
	fmt.Println("acquire resource")
	defer fmt.Println("release resource") // 无论函数怎么退出，都会执行

	fmt.Println("doing work")
}

func main() {
	process()
	// 输出顺序：
	// acquire resource
	// doing work
	// release resource
}
```

### 示例 3：defer 参数求值时机（常见陷阱）

```go
package main

import "fmt"

func main() {
	i := 0
	defer fmt.Println("deferred i =", i) // 参数在 defer 语句执行时立即求值，此时 i=0

	i = 100
	fmt.Println("current i =", i) // 100
	// 输出：
	// current i = 100
	// deferred i = 0
}
```

### 示例 4：多个 defer 的执行顺序——后进先出（LIFO）

```go
package main

import "fmt"

func main() {
	for i := 1; i <= 3; i++ {
		defer fmt.Println("defer", i)
	}
	fmt.Println("main body done")
	// 输出：
	// main body done
	// defer 3
	// defer 2
	// defer 1
}
```

### 示例 5：defer + recover 处理 panic

```go
package main

import "fmt"

func safeDivide(a, b int) (result int, err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("recovered from panic: %v", r)
		}
	}()

	result = a / b // b == 0 时会 panic: integer divide by zero
	return
}

func main() {
	r, err := safeDivide(10, 0)
	fmt.Println(r, err) // 0 recovered from panic: runtime error: integer divide by zero
}
```

### 示例 6：错误包装与 errors.Is

```go
package main

import (
	"errors"
	"fmt"
)

var ErrNotFound = errors.New("not found")

func findUser(id int) error {
	if id != 1 {
		return fmt.Errorf("findUser(%d): %w", id, ErrNotFound) // %w 包装原始错误
	}
	return nil
}

func main() {
	err := findUser(2)
	fmt.Println(err) // findUser(2): not found

	fmt.Println(errors.Is(err, ErrNotFound)) // true，即使被包装过也能判断
}
```

## 快速上手小结

打开资源的下一行就写 `defer xxx.Close()`；对外暴露的错误优先用 `fmt.Errorf("...: %w", err)` 包装，让调用方可以用 `errors.Is`/`errors.As` 判断根因。

---

# 6. Package、Module

## 核心思想

```
Package = 唯一的组织单位，可见性由首字母大小写决定
Module  = 依赖管理边界（go.mod / go.sum）
```

Go 没有 namespace、没有 class，一切都通过 package 组织；Module 是现代 Go 项目管理第三方依赖的标准机制。

## 为什么重要

- 决定项目的目录结构和依赖方向，避免循环依赖
- 首字母大小写是 Go 里"唯一"的访问控制机制，必须内化
- `go.mod` / `go.sum` / `go mod tidy` 是现代 Go 项目的标准工作流
- `internal` 包机制是构建大型项目边界的关键工具

## 代码示例

### 示例 1：可见性——大写导出，小写私有

```go
// file: mathutil/mathutil.go
package mathutil

// Add 是导出函数，包外可以调用
func Add(a, b int) int {
	return add(a, b)
}

// add 是私有函数，只能在 mathutil 包内部调用
func add(a, b int) int {
	return a + b
}
```

```go
// file: main.go
package main

import (
	"fmt"

	"example.com/myapp/mathutil"
)

func main() {
	fmt.Println(mathutil.Add(1, 2)) // OK
	// mathutil.add(1, 2) // 编译错误：add 未导出，包外不可见
}
```

### 示例 2：go.mod 基本结构

```bash
go mod init example.com/myapp
```

```
module example.com/myapp

go 1.22

require (
	github.com/gin-gonic/gin v1.9.1
)
```

### 示例 3：go mod tidy / go get 常用命令

```bash
# 添加一个依赖
go get github.com/google/uuid@latest

# 清理未使用的依赖，补全缺失的依赖，同步 go.sum
go mod tidy

# 查看当前模块的完整依赖图
go list -m all
```

### 示例 4：internal 包——限制跨模块访问

```
example.com/myapp/
├── internal/
│   └── auth/
│       └── auth.go     # 只能被 example.com/myapp/... 下的包导入
├── cmd/
│   └── server/
│       └── main.go     # 可以导入 internal/auth
```

```go
// 其他模块（不在 example.com/myapp 目录树下）尝试导入：
import "example.com/myapp/internal/auth"
// 编译错误：use of internal package example.com/myapp/internal/auth not allowed
```

### 示例 5：包初始化顺序——包级变量先于 init 函数

```go
package main

import "fmt"

var config = loadConfig()

func loadConfig() string {
	fmt.Println("loading config")
	return "config-data"
}

func init() {
	fmt.Println("init: package initialized")
}

func main() {
	fmt.Println("main:", config)
	// 输出顺序：
	// loading config
	// init: package initialized
	// main: config-data
}
```

### 示例 6：依赖方向——避免循环依赖的典型分层

```
project/
├── cmd/server/main.go      -> 依赖 service
├── internal/service/       -> 依赖 repository
├── internal/repository/    -> 依赖 database（不回指 service）
└── internal/database/
```

```go
// repository 包不应该反过来 import service 包，
// 否则会产生编译期错误：import cycle not allowed
```

## 快速上手小结

新建目录前先想清楚"它属于哪一层、依赖方向是否单向"；不希望被外部模块导入的代码放进 `internal/`；日常维护依赖只需要记住 `go get` 和 `go mod tidy` 两个命令。

---

# 回到总览

本文档覆盖了 [core_concepts.md](./core_concepts.md) 优先级表格中 ⭐⭐⭐⭐⭐ 的全部 6 组知识点。接下来可以继续阅读：

- [priority-p1-concurrency.md](./priority-p1-concurrency.md) —— ⭐⭐⭐⭐☆ 并发与标准库
- [priority-p2-runtime.md](./priority-p2-runtime.md) —— ⭐⭐⭐☆☆ Runtime、GC
- [priority-p3-advanced.md](./priority-p3-advanced.md) —— ⭐⭐☆☆☆ reflect、unsafe、cgo
