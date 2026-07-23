# Go —— 组合（Composition）

> **核心驱动力：使用小接口、小组件拼装系统。**
> Go 程序员不问"该继承谁"，而问"该组合哪些小部件"。

---

## 心智模型图解

```
Logger
Config
Database
Cache
      │
      ▼
   Application（把小组件拼装起来）
```

Go 故意没有类继承（class inheritance）。它把"复用"这件事，全部押注在两个机制上：
**接口（interface）的隐式实现** 和 **struct 的嵌入（embedding）**。
系统不是一棵继承树，而是一堆小接口、小结构体互相拼装出来的"积木"。

---

## 核心驱动力详解

- **小接口优于大接口**：`io.Reader` 只有一个方法 `Read`，正因为小，才能被无数类型实现、被无数函数复用。
- **接口是隐式满足的**：类型不需要声明"我实现了 XXX 接口"，只要方法签名对上就自动满足，降低了耦合。
- **组合优于继承**：需要"扩展"一个类型的行为时，Go 鼓励把它作为字段嵌入到新 struct 里，而不是搭建继承树。
- **"接受接口，返回具体类型"**：函数参数尽量用接口（面向行为编程），返回值尽量用具体类型（方便调用者使用全部能力）。

---

## 典型代码片段

### 1. 小接口的力量 —— `io.Reader` / `io.Writer`

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

func Copy(dst Writer, src Reader) (int64, error) {
    // 标准库 io.Copy 的简化版：只依赖两个"一个方法"的接口
    buf := make([]byte, 4096)
    var total int64
    for {
        n, err := src.Read(buf)
        if n > 0 {
            dst.Write(buf[:n])
            total += int64(n)
        }
        if err != nil {
            break
        }
    }
    return total, nil
}
```

**心智模型解读**：`Copy` 不关心 `src` 是文件、网络连接还是内存缓冲区，只要求它"能 Read"。
接口越小，能满足它的类型就越多，函数的复用范围就越广——这是组合思维的地基。

### 2. Struct 嵌入 —— 用组合代替继承

```go
type Base struct {
    ID string
}

func (b Base) Describe() string {
    return "Base#" + b.ID
}

type User struct {
    Base       // 嵌入，不是继承
    Name string
}

u := User{Base: Base{ID: "1"}, Name: "Alice"}
fmt.Println(u.Describe())  // 直接可用，方法被"提升"到 User 上
fmt.Println(u.ID)          // 字段也被提升
```

**心智模型解读**：`User` 并没有"继承" `Base`，而是把一个 `Base` 实例作为匿名字段"塞进"自己内部，
Go 自动把 `Base` 的方法和字段"提升"到 `User` 的方法集上。这是"has-a"，不是"is-a"。

### 3. 接口的隐式满足 —— 不需要显式声明"implements"

```go
type Shape interface {
    Area() float64
}

type Circle struct {
    Radius float64
}

func (c Circle) Area() float64 {
    return 3.14159 * c.Radius * c.Radius
}

func PrintArea(s Shape) {
    fmt.Println(s.Area())
}

PrintArea(Circle{Radius: 2})  // Circle 从未写过 "implements Shape"
```

**心智模型解读**：只要 `Circle` 拥有签名匹配的 `Area()` 方法，它就自动满足 `Shape` 接口。
这种"结构化类型"让组合可以发生在完全不相关的包之间，不需要提前约定继承关系。

### 4. "接受接口，返回结构体"

```go
func NewFileLogger(path string) (*FileLogger, error) { // 返回具体类型
    f, err := os.Create(path)
    if err != nil {
        return nil, err
    }
    return &FileLogger{file: f}, nil
}

func Process(w io.Writer) { // 参数接受接口
    fmt.Fprintln(w, "processing...")
}
```

**心智模型解读**：构造函数返回具体类型，让调用者能用上这个类型的全部方法；
而处理函数接受接口，让自己可以被任意满足该接口的类型驱动。这是 Go 里公认的组合惯例。

### 5. 函数式选项模式（Functional Options）—— 用组合构造复杂配置

```go
type ServerOption func(*Server)

func WithPort(port int) ServerOption {
    return func(s *Server) { s.port = port }
}

func WithTimeout(d time.Duration) ServerOption {
    return func(s *Server) { s.timeout = d }
}

func NewServer(opts ...ServerOption) *Server {
    s := &Server{port: 8080, timeout: 30 * time.Second} // 默认值
    for _, opt := range opts {
        opt(s) // 逐个应用小的"配置组件"
    }
    return s
}

srv := NewServer(WithPort(9090), WithTimeout(5*time.Second))
```

**心智模型解读**：每个 `Option` 都是一个独立、可测试的小组件，`NewServer` 只负责把它们
按顺序组合应用。这避免了"构造函数参数爆炸"，本质上是"组合"思维在 API 设计层面的体现。

### 6. Goroutine + Channel —— 用小的并发单元组合出流水线

```go
func generate(nums ...int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for _, n := range nums {
            out <- n
        }
    }()
    return out
}

func square(in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for n := range in {
            out <- n * n
        }
    }()
    return out
}

for result := range square(generate(1, 2, 3, 4)) {
    fmt.Println(result) // 1 4 9 16
}
```

**心智模型解读**：`generate` 和 `square` 都是独立的小组件，每个只做一件事，
通过 channel 串接成一条流水线——这和 Shell 管道的思维几乎一致，只是换成了并发原语。

### 7. 错误包装 —— 组合出带上下文的错误链

```go
func loadConfig(path string) (*Config, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("loadConfig: read %s: %w", path, err) // 包装，不吞掉原始错误
    }
    // ...
    return cfg, nil
}

// 调用方可以用 errors.Is / errors.As 判断底层原因
if errors.Is(err, os.ErrNotExist) {
    // 处理文件不存在
}
```

**心智模型解读**：`%w` 把底层错误"组合"进新的错误里，形成一条可追溯的错误链，
而不是像继承体系那样定义一堆异常子类。这也是"组合优于继承"在错误处理上的延伸。

### 8. 中间件模式 —— 用函数组合包装 `http.Handler`

```go
type Middleware func(http.Handler) http.Handler

func Logging(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        log.Println(r.Method, r.URL.Path)
        next.ServeHTTP(w, r) // 组合：调用下一个 handler
    })
}

func Chain(h http.Handler, mws ...Middleware) http.Handler {
    for i := len(mws) - 1; i >= 0; i-- {
        h = mws[i](h)
    }
    return h
}

finalHandler := Chain(baseHandler, Logging, Auth, RateLimit)
```

**心智模型解读**：每个中间件都是"包一层"的小组件，`Chain` 把它们像俄罗斯套娃一样组合起来。
没有基类、没有继承树，只有"函数包装函数"这一种组合方式。

### 9. 组合出更大的接口 —— `io.ReadWriter`

```go
type ReadWriter interface {
    Reader
    Writer
}
```

**心智模型解读**：Go 允许接口嵌入接口，把两个小接口"拼"成一个大接口。
这和 struct 嵌入是同一种哲学的两面：小的、正交的能力单元，可以按需组合出更大的能力。

### 10. 用组合替代"子类特殊化"

```go
type Animal struct {
    Name string
}

func (a Animal) Eat() string {
    return a.Name + " is eating"
}

type FlyBehavior struct{}
func (FlyBehavior) Fly() string { return "flying" }

type Bird struct {
    Animal
    FlyBehavior
}

b := Bird{Animal: Animal{Name: "Sparrow"}}
fmt.Println(b.Eat())  // 来自 Animal
fmt.Println(b.Fly())  // 来自 FlyBehavior
```

**心智模型解读**：`Bird` 的"能飞"能力不是通过继承一个 `FlyableAnimal` 基类获得的，
而是把 `FlyBehavior` 作为独立组件嵌入进来——不同动物可以自由挑选要嵌入哪些行为组件。

### 11. 空接口 + 类型断言 —— 组合边界处的"降级"处理

```go
func describe(v interface{}) string {
    switch x := v.(type) {
    case fmt.Stringer:
        return x.String()
    case error:
        return x.Error()
    default:
        return fmt.Sprintf("%v", x)
    }
}
```

**心智模型解读**：在真正不知道类型的边界上（比如反序列化后的数据），Go 用类型断言/`switch`
逐层探测"这个值支持哪些协议/接口"，本质上和小接口思维一致：先问"它能做什么"，再决定怎么处理。

---

## 黄金法则

> **不要想着继承，而要想着组合。**

需要复用某种能力时，先问："能不能把它定义成一个小接口，或者一个可以被嵌入的小 struct？"
而不是问："我该设计一个什么样的基类，让子类去继承？"

---

## 常见误区对比

### 误区一：设计"上帝接口"

```go
// 错误心智模型：把很多不相关的方法塞进一个接口
type Repository interface {
    Save(x interface{}) error
    Delete(id string) error
    FindByID(id string) (interface{}, error)
    FindAll() ([]interface{}, error)
    Backup() error
    SendNotification() error
}
```

```go
// Go 习惯写法：拆成多个小接口，按需组合
type Saver interface{ Save(x interface{}) error }
type Finder interface{ FindByID(id string) (interface{}, error) }

func Process(s Saver, f Finder) { /* 只依赖需要的能力 */ }
```

**为什么后者更好**：调用方只需要实现/依赖自己真正用到的那部分能力，
mock/测试也只需要实现最小接口，耦合面积大幅缩小。

### 误区二：用嵌入模拟"多态覆写"，却忘记 Go 没有虚方法分派

```go
type Animal struct{}
func (Animal) Speak() string { return "..." }

type Dog struct {
    Animal
}
func (Dog) Speak() string { return "Woof" }

func callSpeak(a Animal) {
    fmt.Println(a.Speak()) // 永远打印 "..."，不会因为传入 Dog 而变成 "Woof"
}

callSpeak(Dog{}.Animal) // 传的是内部的 Animal 字段，丢失了 Dog 的方法
```

```go
// Go 习惯写法：如果需要"多态"，用接口而不是嵌入
type Speaker interface{ Speak() string }

func callSpeak(s Speaker) {
    fmt.Println(s.Speak())
}

callSpeak(Dog{}) // 打印 "Woof"，因为接口在运行时按具体类型分派方法
```

**为什么后者更好**：嵌入只是"字段+方法提升"的语法糖，没有虚函数表；
真正需要"运行时根据具体类型调用不同实现"时，必须用接口，而不是嵌入。

---

## 快速上手 Checklist

- [ ] 设计接口时，是否克制住了"多加几个方法"的冲动，尽量让接口保持小而正交？
- [ ] 需要复用某个类型的字段/方法时，第一反应是嵌入（组合），而不是想"能不能继承"？
- [ ] 函数参数是否倾向于用接口（描述"需要什么能力"），返回值倾向于用具体类型？
- [ ] 是否理解嵌入不等于多态，需要动态分派时该用接口？
- [ ] 遇到复杂配置构造场景时，是否想到过函数式选项模式？

---

上一篇：[Python —— 协议](python.md) ・ 下一篇：[Rust —— 所有权](rust.md)
