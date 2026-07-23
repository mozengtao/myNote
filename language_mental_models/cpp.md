# C++ —— 生命周期（Lifetime）

> **核心驱动力：资源跟随对象生命周期自动管理。**
> C++ 程序员不问"什么时候手动释放"，而问"这个资源绑定在哪个对象的生命周期上"。

---

## 心智模型图解

```
Construct（构造：获取资源）
      │
      ▼
Use Resource（在对象存活期间使用资源）
      │
      ▼
Destruct（析构：作用域结束，自动释放资源）
```

C++ 继承了 C 的"手动内存管理"，但发明了一套机制，把"资源的生命周期"绑定到"对象的生命周期"上——
这就是 RAII（Resource Acquisition Is Initialization）。只要对象按规则构造/析构，资源就能被自动、
确定性地释放，不需要 `finally`，也不需要垃圾回收。

---

## 核心驱动力详解

- **构造函数负责获取资源，析构函数负责释放资源**：文件、锁、内存、网络连接，都可以包一层"生命周期管理对象"。
- **作用域即生命周期**：一个局部对象在其作用域结束时（正常返回、异常抛出、`break`/`return` 都算）一定会被析构。
- **移动语义把"生命周期的转移"变成显式操作**：`std::move` 表示"我把资源的所有权交给你，我自己不再管它"。
- **智能指针把手动 `new`/`delete` 封装成 RAII 对象**，让"忘记释放"这类问题在正常使用下几乎消失。

---

## 典型代码片段

### 1. RAII 最经典的应用 —— `std::lock_guard`

```cpp
#include <mutex>

std::mutex mtx;

void increment(int &counter) {
    std::lock_guard<std::mutex> lock(mtx); // 构造时加锁
    ++counter;
} // lock 在这里析构，自动解锁——即使中间抛异常也一样
```

**心智模型解读**：不需要手动写 `mtx.unlock()`，无论函数正常返回还是异常退出，
`lock` 的析构函数都会被调用，锁一定会被释放。生命周期管理把"异常安全"变成了免费的副产品。

### 2. 构造/析构配对 —— 自定义资源管理类

```cpp
class FileHandle {
public:
    explicit FileHandle(const char *path) : fp_(std::fopen(path, "r")) {
        if (!fp_) throw std::runtime_error("open failed");
    }
    ~FileHandle() {
        if (fp_) std::fclose(fp_);
    }
    FILE *get() const { return fp_; }
private:
    FILE *fp_;
};

void read_file(const char *path) {
    FileHandle f(path); // 构造 = 打开文件
    // 使用 f.get() ...
} // f 离开作用域，析构函数自动关闭文件
```

**心智模型解读**：`FileHandle` 把"文件描述符"这个裸资源，绑定到一个 C++ 对象的生命周期上，
从此"忘记关闭文件"这个错误，只要按正常方式使用局部变量，就不可能发生。

### 3. `unique_ptr` —— 独占所有权的智能指针

```cpp
#include <memory>

std::unique_ptr<int> make_value() {
    return std::make_unique<int>(42);
}

int main() {
    auto p = make_value(); // p 独占这块堆内存的生命周期
    std::cout << *p << "\n";
} // p 离开作用域，自动 delete，无需手写 delete
```

**心智模型解读**：`unique_ptr` 就是"把 `new`/`delete` 包进一个 RAII 对象"，
它不能被拷贝（拷贝构造被删除），只能被移动——这保证了"堆内存永远只有一个生命周期归属者"。

### 4. `shared_ptr` —— 引用计数的共享生命周期

```cpp
#include <memory>

struct Node {
    int value;
    std::shared_ptr<Node> next;
};

int main() {
    auto a = std::make_shared<Node>(Node{1, nullptr});
    auto b = a; // 引用计数从 1 变为 2，a、b 共享同一份生命周期

    std::cout << a.use_count() << "\n"; // 2
} // a、b 依次离开作用域，引用计数减到 0 时对象才被真正销毁
```

**心智模型解读**：当"谁最后离开作用域就该释放资源"这件事无法由单一对象决定时，
`shared_ptr` 用引用计数把"生命周期终点"从"某一个固定作用域"，变成"最后一个持有者离开的那一刻"。

### 5. 移动构造与 `std::move` —— 显式转移生命周期所绑定的资源

```cpp
class Buffer {
public:
    Buffer(size_t n) : data_(new int[n]), size_(n) {}

    Buffer(Buffer &&other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr; // 原对象放弃对资源的所有权
        other.size_ = 0;
    }

    ~Buffer() { delete[] data_; }

private:
    int *data_;
    size_t size_;
};

Buffer make_big_buffer() {
    Buffer b(1000000);
    return b; // 编译器倾向于用移动构造，而不是深拷贝整个数组
}
```

**心智模型解读**：移动构造把"资源指针"直接搬到新对象身上，并让旧对象的指针置空，
这样旧对象析构时不会重复释放（double free），资源的生命周期被"接力"给了新对象。

### 6. 作用域自动清理 —— 生命周期与代码块严格绑定

```cpp
void process() {
    std::ofstream log("run.log"); // 构造时打开文件
    log << "start\n";

    if (some_error_condition()) {
        return; // 提前返回，log 依然会被正确析构、关闭文件
    }

    log << "done\n";
} // 正常路径下 log 也会在这里析构
```

**心智模型解读**：不管函数从哪条路径退出（正常结束、提前 `return`、抛异常），
局部对象的析构顺序都是"后构造先析构"，严格遵循作用域嵌套关系——这是 C++ 生命周期模型最可靠的保证。

### 7. 拷贝构造与深拷贝 —— 明确"复制"是否意味着"复制资源"

```cpp
class DeepArray {
public:
    DeepArray(size_t n) : size_(n), data_(new int[n]{}) {}

    DeepArray(const DeepArray &other) : size_(other.size_), data_(new int[other.size_]) {
        std::copy(other.data_, other.data_ + size_, data_); // 深拷贝，不共享生命周期
    }

    ~DeepArray() { delete[] data_; }

private:
    size_t size_;
    int *data_;
};
```

**心智模型解读**：如果不自定义拷贝构造函数，编译器生成的默认版本只会"浅拷贝"指针，
导致两个对象在析构时对同一块内存各自 `delete` 一次（double free）。生命周期一旦涉及裸资源，
拷贝语义必须被显式定义清楚："复制对象"和"复制资源"是两件不同的事。

### 8. 异常安全 —— RAII 让异常路径也能正确释放资源

```cpp
void risky_operation() {
    std::unique_ptr<Widget> w = std::make_unique<Widget>();
    w->init(); // 假设这里抛出异常
    w->run();
} // 无论 init() 是否抛异常，w 都会在栈展开（stack unwinding）过程中被正确析构
```

**心智模型解读**：如果用裸指针 + 手动 `delete`，异常抛出会跳过 `delete` 语句，造成内存泄漏；
而 RAII 对象的析构由语言保证在栈展开时被调用，异常安全几乎是"顺带"实现的，不需要额外的 `catch`/`finally`。

### 9. 自定义 RAII 包装类 —— 把任意资源纳入生命周期管理

```cpp
template <typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F f) : f_(std::move(f)) {}
    ~ScopeGuard() { f_(); } // 离开作用域时执行任意"清理动作"
private:
    F f_;
};

void connect_and_use() {
    auto conn = open_connection();
    ScopeGuard guard([&conn] { close_connection(conn); }); // 把"关闭"绑定到 guard 的生命周期

    use_connection(conn);
} // guard 析构，自动执行 close_connection
```

**心智模型解读**：RAII 不局限于内存/锁/文件这些"内建"资源，任何"需要成对出现的操作"
（打开/关闭、开始/结束、订阅/取消订阅），都可以用一个只在析构函数里执行清理逻辑的对象来管理。

### 10. Rule of Five —— 一旦手动管理资源，就要显式定义全部特殊成员函数

```cpp
class Resource {
public:
    Resource() = default;
    ~Resource();                                    // 析构函数
    Resource(const Resource &);                      // 拷贝构造
    Resource &operator=(const Resource &);           // 拷贝赋值
    Resource(Resource &&) noexcept;                   // 移动构造
    Resource &operator=(Resource &&) noexcept;        // 移动赋值
private:
    int *data_ = nullptr;
};
```

**心智模型解读**：一旦一个类需要自定义析构函数（说明它管理着某种裸资源），
就必须同时考虑拷贝和移动这两大类共 4 个操作该如何定义生命周期的转移/复制规则，
否则编译器生成的默认版本很可能在某个路径上重复释放或忘记释放资源。

### 11. 局部静态变量 —— 生命周期跨越单次函数调用，但仍受程序结束约束

```cpp
Logger &get_logger() {
    static Logger instance; // 第一次调用时构造，程序结束时析构（而不是函数返回时）
    return instance;
}
```

**心智模型解读**：`static` 局部变量把生命周期从"函数调用的作用域"延长到了"整个程序运行期"，
但依然遵守"有构造就有析构"的原则——只是析构被推迟到了 `main` 结束之后的全局清理阶段。

---

## 黄金法则

> **不要想着什么时候释放资源，而要想着生命周期。**

看到一个资源（内存、锁、文件、连接），先问："它应该绑定在哪个对象身上？
这个对象的作用域什么时候结束？"而不是到处手写配对的"获取/释放"语句。

---

## 常见误区对比

### 误区一：手动 `new`/`delete`，依赖"记得写对称的释放语句"

```cpp
// 错误心智模型：把资源管理当成"记忆力任务"
void process() {
    Widget *w = new Widget();
    w->init();
    if (w->hasError()) {
        return; // 忘记 delete w，内存泄漏
    }
    w->run();
    delete w;
}
```

```cpp
// C++ 习惯写法：交给 RAII 对象，生命周期自动跟随作用域
void process() {
    auto w = std::make_unique<Widget>();
    w->init();
    if (w->hasError()) {
        return; // w 离开作用域时自动析构，不会泄漏
    }
    w->run();
}
```

**为什么后者更好**：智能指针把"释放"这件事从"程序员必须在每条退出路径上都记得写一遍"，
变成了"语言保证，只要作用域结束就自动执行一次"，从根源上消除了大部分资源泄漏。

### 误区二：只定义了拷贝构造，没有一并处理拷贝赋值/移动语义

```cpp
// 错误心智模型：以为只要写了拷贝构造函数就够了
class Buffer {
public:
    Buffer(const Buffer &other) : data_(new int[other.size_]) { /* ... */ }
    ~Buffer() { delete[] data_; }
    // 没有定义 operator=，编译器生成的默认拷贝赋值会浅拷贝 data_，导致 double free
private:
    int *data_;
    size_t size_;
};
```

```cpp
// C++ 习惯写法：拷贝构造、拷贝赋值、析构一起定义（至少三者一致），或者用 = delete 明确禁止
class Buffer {
public:
    Buffer(const Buffer &other) : data_(new int[other.size_]), size_(other.size_) { /* 深拷贝 */ }
    Buffer &operator=(const Buffer &other) {
        if (this == &other) return *this;
        delete[] data_;
        data_ = new int[other.size_];
        size_ = other.size_;
        /* 深拷贝内容 */
        return *this;
    }
    ~Buffer() { delete[] data_; }
private:
    int *data_;
    size_t size_;
};
```

**为什么后者更好**：一旦类里出现裸资源，拷贝构造、拷贝赋值、析构函数（以及移动版本）
必须作为一套完整的生命周期规则一起考虑，只定义其中一部分，另一部分会用默认的"浅拷贝"埋下隐患。

---

## 快速上手 Checklist

- [ ] 看到一个需要"配对操作"的资源（打开/关闭），第一反应是包一个 RAII 类，而不是手写配对语句吗？
- [ ] 能分清 `unique_ptr`（独占生命周期）和 `shared_ptr`（共享生命周期，引用计数）的适用场景吗？
- [ ] 理解为什么"函数返回局部对象"通常会触发移动构造而不是深拷贝吗？
- [ ] 自定义类管理裸资源时，是否想到了 Rule of Five（或者干脆 `= delete` 拷贝，只允许移动）？
- [ ] 能说出为什么 RAII 能让异常安全"几乎免费"实现吗？

---

上一篇：[C —— 内存](c.md) ・ 下一篇：[Java —— 职责](java.md)
