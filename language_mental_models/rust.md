# Rust —— 所有权（Ownership）

> **核心驱动力：数据必须有明确的拥有者。**
> Rust 程序员不问"这块内存什么时候释放"，而问"这个值现在归谁所有，谁有权借用它"。

---

## 心智模型图解

```
Data（数据）
     │
     ▼
Owner（唯一拥有者）
     │
     ├── Borrow（借用：&/&mut，临时授权，不转移所有权）
     └── Move（转移：所有权整体转手，原变量失效）
```

Rust 编译器在编译期就要回答一个问题："这块数据，此刻到底归谁？"
一旦某个变量拥有了数据，它就要负责在自己作用域结束时释放数据（类似 C++ 的 RAII）；
把数据传给别人，要么是"借"（临时、可撤回），要么是"转移"（永久、原主失去访问权）。

---

## 核心驱动力详解

- **每个值只有一个所有者**：赋值、传参、返回值默认都是"移动"（Move），不是拷贝。
- **借用不转移所有权**：`&T` 是只读借用，`&mut T` 是独占可写借用，编译器保证同一时刻不会有"读写冲突"。
- **生命周期（lifetime）是所有权在时间维度上的延伸**：借用不能比它所借的数据活得更久。
- **Drop 是所有权终点的仪式**：当所有者离开作用域，Rust 自动调用 `Drop::drop`，无需手动 `free`。

一旦建立"谁拥有、谁借用、借多久"的思维，Rust 的大多数编译错误（借用检查器报错）都会变得可预测。

---

## 典型代码片段

### 1. Move 语义 —— 赋值即转移所有权

```rust
fn main() {
    let s1 = String::from("hello");
    let s2 = s1; // s1 的所有权被"移动"给 s2

    // println!("{}", s1); // 编译错误：s1 已失效（value borrowed after move）
    println!("{}", s2);    // OK
}
```

**心智模型解读**：`String` 没有实现 `Copy`，所以 `let s2 = s1` 不是复制一份数据，
而是把"所有权"整体转移给 `s2`，`s1` 从此不再是合法的访问入口。

### 2. 不可变借用 `&` —— 临时"借"而不拥有

```rust
fn calculate_length(s: &String) -> usize {
    s.len() // 只是借用，函数结束不会释放 s 指向的数据
}

fn main() {
    let s1 = String::from("hello");
    let len = calculate_length(&s1);
    println!("{} has length {}", s1, len); // s1 仍然有效
}
```

**心智模型解读**：把 `&s1` 传给函数，只是"借出"访问权，函数用完即还，
所有权仍在 `main` 里的 `s1` 手中——这就是为什么函数结束后 `s1` 还能用。

### 3. 可变借用 `&mut` —— 独占的写权限

```rust
fn add_suffix(s: &mut String) {
    s.push_str("_v2");
}

fn main() {
    let mut s = String::from("report");
    add_suffix(&mut s);
    println!("{}", s); // report_v2
}
```

**心智模型解读**：`&mut` 借用期间，其他任何借用（读或写）都不能同时存在——
编译器在编译期就替你排除了"数据竞争"的可能性，这是所有权模型延伸出的并发安全保证。

### 4. 借用检查器如何阻止悬垂引用

```rust
fn dangling() -> &String { // 编译错误：missing lifetime specifier
    let s = String::from("hello");
    &s // s 在函数结束时被 drop，返回它的引用会变成悬垂引用
}
```

```rust
fn not_dangling() -> String {
    let s = String::from("hello");
    s // 直接返回所有权，没有引用悬垂的问题
}
```

**心智模型解读**：借用不能比它所借的数据活得更久。这里 `s` 在函数结束时所有权到期、被 `drop`，
如果还想把引用带出函数，编译器会直接拒绝——这正是"借用不能超过所有者的生命周期"的体现。

### 5. 生命周期标注 `'a` —— 显式声明"借用关系必须活多久"

```rust
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}

fn main() {
    let s1 = String::from("long string");
    let result;
    {
        let s2 = String::from("short");
        result = longest(s1.as_str(), s2.as_str());
        println!("{}", result); // 必须在 s2 的作用域内使用 result
    }
}
```

**心智模型解读**：`'a` 告诉编译器"返回的引用，寿命不能超过 `x` 和 `y` 中较短的那个"。
生命周期标注不是"发明"新规则，而是把本来隐含的"借用有效期"显式写出来，供编译器核对。

### 6. `Box<T>` —— 把数据的所有权放到堆上

```rust
struct Node {
    value: i32,
    next: Option<Box<Node>>, // 递归结构必须通过 Box 间接持有，否则大小无限
}

let list = Node {
    value: 1,
    next: Some(Box::new(Node { value: 2, next: None })),
};
```

**心智模型解读**：`Box<Node>` 仍然只有一个所有者，只是数据存储在堆上；
所有权规则不因为"堆/栈"而改变，`Box` 只是解决了"值大小不确定"或"需要间接引用自身类型"的问题。

### 7. `Rc<RefCell<T>>` —— 共享所有权 + 运行期可变性

```rust
use std::rc::Rc;
use std::cell::RefCell;

let shared = Rc::new(RefCell::new(vec![1, 2, 3]));

let a = Rc::clone(&shared); // 增加引用计数，而不是深拷贝数据
let b = Rc::clone(&shared);

a.borrow_mut().push(4);
println!("{:?}", b.borrow()); // [1, 2, 3, 4]，a、b 看到的是同一份数据
```

**心智模型解读**：当"单一所有者"无法满足需求（比如多个节点都要引用同一份数据）时，
Rust 提供 `Rc` 把"独占所有权"降级为"共享所有权+引用计数"，`RefCell` 再把借用检查从编译期挪到运行期。

### 8. `Drop` —— 所有权终结时的自动清理（Rust 版 RAII）

```rust
struct Guard {
    name: String,
}

impl Drop for Guard {
    fn drop(&mut self) {
        println!("releasing {}", self.name);
    }
}

fn main() {
    let _g1 = Guard { name: "first".into() };
    {
        let _g2 = Guard { name: "second".into() };
        println!("inside inner scope");
    } // _g2 在这里被 drop -> "releasing second"
    println!("inside outer scope");
} // _g1 在这里被 drop -> "releasing first"
```

**心智模型解读**：不需要手动调用释放函数，只要变量的所有权在某个作用域结束，
Rust 保证自动调用 `Drop::drop`——这是所有权模型天然带来的确定性资源释放。

### 9. `Clone` vs `Move` —— 显式选择"复制"还是"转移"

```rust
#[derive(Clone)]
struct Config {
    name: String,
}

fn main() {
    let c1 = Config { name: "prod".into() };
    let c2 = c1.clone(); // 显式深拷贝，c1、c2 各自拥有独立数据

    println!("{} {}", c1.name, c2.name); // 两者都可用
}
```

**心智模型解读**：Rust 默认是 Move（转移），如果你想保留原变量继续可用，必须显式 `.clone()`。
这个"显式"是有意为之：让复制的成本在代码里"看得见"，而不是像某些语言那样悄悄发生。

### 10. 所有权在函数间的传递与归还

```rust
fn take_and_give_back(s: String) -> String {
    println!("processing: {}", s);
    s // 把所有权还给调用者
}

fn main() {
    let s1 = String::from("data");
    let s1 = take_and_give_back(s1); // 所有权：main -> 函数 -> 又回到 main
    println!("{}", s1);
}
```

**心智模型解读**：所有权可以像"接力棒"一样在函数调用链中传递，
每一步都能明确回答"此刻这份数据归谁"，这是 Rust 强制你随时维护的一张"所有权地图"。

### 11. 切片借用 `&[T]` —— 借用一段连续数据而不获取所有权

```rust
fn sum(slice: &[i32]) -> i32 {
    slice.iter().sum()
}

fn main() {
    let v = vec![1, 2, 3, 4, 5];
    let total = sum(&v[1..4]); // 借用 v 的一部分，不拷贝、不转移所有权
    println!("{}", total); // 2+3+4=9
    println!("{:?}", v);   // v 依然完整可用
}
```

**心智模型解读**：切片是"借用一个视图"，既不复制数据也不改变所有权归属，
`v` 全程仍是数据的唯一所有者，`sum` 只是临时看了一眼其中一段。

### 12. `match` + `Option`/`Result` —— 用所有权安全地处理"可能没有值"

```rust
fn find_user(id: u32) -> Option<String> {
    if id == 1 { Some(String::from("Alice")) } else { None }
}

fn main() {
    match find_user(1) {
        Some(name) => println!("found: {}", name), // name 获得了内部 String 的所有权
        None => println!("not found"),
    }
}
```

**心智模型解读**：`Option<String>` 里的 `String` 也遵守所有权规则——`match` 到 `Some(name)`
分支时，`name` 拿到了内部字符串的所有权，这也是为什么 Rust 用 `Option`/`Result` 而不是空指针/异常来表达"可能失败"。

---

## 黄金法则

> **不要想着复制数据，而要想着所有权如何流转。**

每次给函数传参、返回值、赋值，先问自己："这里是移动、借用（只读），还是借用（可写）？"
而不是先假设"变量都是复制传递"（像很多脚本语言默认那样）。

---

## 常见误区对比

### 误区一：试图在一个作用域里同时持有可变借用和不可变借用

```rust
// 错误心智模型：以为只要变量还在作用域内，随时都能再借用
fn main() {
    let mut v = vec![1, 2, 3];
    let first = &v[0];
    v.push(4); // 编译错误：cannot borrow `v` as mutable because it is also borrowed as immutable
    println!("{}", first);
}
```

```rust
// Rust 习惯写法：缩短不可变借用的生命周期，避免与可变借用重叠
fn main() {
    let mut v = vec![1, 2, 3];
    {
        let first = &v[0];
        println!("{}", first);
    } // first 的借用在这里结束
    v.push(4); // OK
}
```

**为什么后者更好**：借用检查器要求"同一时刻，只能有一个可变借用，或者任意多个不可变借用"，
提前结束不可变借用的作用域，就不会和后面的可变操作冲突——这正是编译期避免数据竞争的机制。

### 误区二：把 Move 语义误当成"引用传递"

```rust
// 错误心智模型：以为传入函数后原变量依然可用（像 Python/Java 传引用一样）
fn consume(v: Vec<i32>) {
    println!("{:?}", v);
}

fn main() {
    let v = vec![1, 2, 3];
    consume(v);
    println!("{:?}", v); // 编译错误：value borrowed here after move
}
```

```rust
// Rust 习惯写法：如果调用后还要用，就借用而不是转移
fn consume(v: &Vec<i32>) {
    println!("{:?}", v);
}

fn main() {
    let v = vec![1, 2, 3];
    consume(&v);
    println!("{:?}", v); // OK，所有权一直在 main 手里
}
```

**为什么后者更好**：明确表达"这个函数只是看一眼数据，不需要拿走它"，
调用者不需要为了让代码能编译而被迫写 `.clone()`，也不会误以为函数会一直保留这份数据。

---

## 快速上手 Checklist

- [ ] 看到一次赋值/传参，能立刻判断这是 Move、`&` 借用还是 `&mut` 借用吗？
- [ ] 遇到"cannot borrow as mutable/immutable"报错时，能画出这块数据当前的借用关系图吗？
- [ ] 写函数签名时，会优先考虑"这个参数需要拥有数据，还是只需要借用"？
- [ ] 理解 `Box`/`Rc`/`RefCell` 分别在解决"堆分配"、"共享所有权"、"运行期可变性"里的哪一个问题？
- [ ] 能说出为什么 Rust 不需要垂手动 `free`/GC，也不会有悬垂指针？

---

上一篇：[Go —— 组合](go.md) ・ 下一篇：[C —— 内存](c.md)
