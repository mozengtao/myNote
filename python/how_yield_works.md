# Python Generator、yield 与 PyFrameObject 心智模型

> **一句话理解**

普通函数：
> 调用一次 → 创建栈帧 → 一直执行到 return → 栈帧销毁。

Generator：
> 调用一次 → 创建栈帧 → 执行到 yield 暂停 → 栈帧保留 → 下次 next() 从暂停点继续执行。

所以：

> **yield 的本质就是："暂停当前 Frame（PyFrameObject），以后继续执行。"**

---

# 一、一个最经典的 Generator 示例

```python
def counter():
    x = 10
    print("A", x)

    yield x

    x += 1
    print("B", x)

    yield x

    x += 1
    print("C", x)

    return
```

创建 Generator：

```python
g = counter()
```

注意：

此时函数并没有运行。

只是创建了：

```
GeneratorObject
        │
        ▼
PyFrameObject
```

---

# 二、Generator Object 内部

Python 中：

```
g
```

实际上是：

```
PyGenObject
```

它里面最重要的成员就是：

```
g
│
├── gi_frame
├── gi_code
├── gi_running
└── ...
```

最重要的是：

```
gi_frame
```

因为：

> Generator 的整个运行状态，全都保存在 Frame 中。

---

# 三、第一次 next()

```python
next(g)
```

执行流程：

```
counter()

↓

x = 10

↓

print()

↓

yield x
```

返回：

```
10
```

但是函数没有结束。

而是：

```
暂停
```

于是：

```
Generator
      │
      ▼
PyFrameObject
```

仍然存在。

---

# 四、此时 Frame 保存了什么？

看看：

```python
g.gi_frame
```

输出类似：

```
<frame at 0x..., file demo.py, line 6, code counter>
```

说明：

Generator 真的拥有一个 Frame。

---

# 五、查看局部变量

```python
g.gi_frame.f_locals
```

得到：

```python
{
    'x': 10
}
```

说明：

局部变量没有消失。

为什么？

因为 Frame 根本没有销毁。

它一直存在。

所以：

```
yield

不是：

return

而是：

Pause
```

---

# 六、查看当前执行位置

看看：

```python
g.gi_frame.f_lasti
```

例如：

```
42
```

很多人不知道：

这个数字是什么？

其实：

```
f_lasti
```

就是：

> 当前执行到哪个 Bytecode。

可以理解成：

```
Instruction Pointer
(IP)
```

类似：

CPU：

```
RIP
```

或者：

C：

```
Program Counter
```

Python Frame 中：

```
f_lasti
```

记录：

```
下一次从哪里继续执行。
```

---

# 七、再次 next()

继续：

```python
next(g)
```

Generator 并不会重新开始。

而是：

直接跳到：

```
yield 后面
```

执行：

```
x += 1

↓

print()

↓

yield x
```

返回：

```
11
```

此时：

```
f_locals
```

变成：

```python
{
    'x': 11
}
```

说明：

局部变量一直保存着。

---

# 八、最后一次 next()

继续：

```python
next(g)
```

执行：

```
yield 后

↓

x += 1

↓

return
```

结束。

抛出：

```
StopIteration
```

此时：

```
g.gi_frame
```

变成：

```python
None
```

为什么？

因为：

Frame 已经释放了。

Generator 生命周期结束。

---

# 九、整个生命周期图

## 创建 Generator

```
counter()

↓

Generator
        │
        ▼
 Frame
```

Frame：

```
未执行
```

---

## 第一次 next()

```
next()

↓

执行

↓

yield
```

Frame：

```
保留
```

状态：

```
locals

x=10
```

---

## 第二次 next()

```
恢复

↓

继续执行

↓

yield
```

Frame：

```
继续保存
```

locals：

```
x=11
```

---

## 第三次 next()

```
恢复

↓

return

↓

Frame 销毁
```

最终：

```
gi_frame

↓

None
```

---

# 十、整个 Frame 的变化

开始：

```
Generator

↓

Frame

↓

f_locals = {}

f_lasti = -1
```

第一次 yield：

```
Frame

↓

f_locals

x=10

↓

f_lasti

指向第一个 yield 后的位置
```

第二次 yield：

```
Frame

↓

x=11

↓

f_lasti

指向第二个 yield 后的位置
```

结束：

```
Frame

↓

释放
```

---

# 十一、为什么 Generator 能恢复？

秘诀只有一句：

> **整个函数调用栈(Frame)没有销毁。**

普通函数：

```
foo()

↓

Frame

↓

return

↓

Frame Free
```

Generator：

```
foo()

↓

Frame

↓

yield

↓

Frame Keep
```

恢复：

```
next()

↓

继续使用旧 Frame
```

所以：

Generator 不需要重新创建变量。

因为：

```
局部变量

循环变量

try

except

递归状态

调用位置

全部保存在 Frame 中。
```

---

# 十二、PyFrameObject 中最重要的几个成员

对于 Generator，最值得观察的是：

| 属性 | 含义 | 是否保留 |
|------|------|----------|
| `f_locals` | 局部变量 | ✅ |
| `f_globals` | 全局变量 | ✅ |
| `f_builtins` | 内建对象 | ✅ |
| `f_code` | Code Object | ✅ |
| `f_lasti` | 当前执行到哪条 Bytecode | ✅ |
| `f_lineno` | 当前源码行号 | ✅ |
| `f_back` | 调用者 Frame | 部分场景存在 |

其中：

```
f_locals
```

保存：

```
所有局部变量
```

例如：

```python
{
    'x':10,
    'i':5,
    'user':<User>,
}
```

而：

```
f_lasti
```

保存：

```
恢复执行的位置
```

---

# 十三、Generator 的运行状态

Generator 其实只有四种状态。

```
Created
```

```
counter()

↓

Generator

↓

Frame 创建
```

---

```
Running
```

```
next()

↓

执行代码
```

---

```
Suspended
```

```
yield

↓

Frame 保留
```

此时：

```
g.gi_frame
```

有效。

---

```
Closed
```

```
return

↓

StopIteration

↓

Frame=None
```

---

# 十四、与普通函数的本质区别

普通函数：

```
call

↓

Frame

↓

执行

↓

return

↓

Frame 销毁
```

Generator：

```
call

↓

Frame

↓

yield

↓

Frame 保留

↓

next()

↓

继续

↓

yield

↓

继续

↓

return

↓

Frame 销毁
```

因此，**Generator 并不是"保存变量"那么简单，而是保存了整个函数执行现场（Execution Context）**。

---

# 十五、终极心智模型（Mental Model）

可以把 Generator 看成一个**可暂停、可恢复的虚拟线程（轻量级协程）**：

```
                Generator (PyGenObject)
                        │
                        ▼
              +--------------------+
              |   gi_frame         |
              |--------------------|
              | f_locals           |──► x=10
              | f_lasti            |──► 下一条字节码位置
              | f_code             |──► counter() 的 CodeObject
              | f_globals          |
              | f_builtins         |
              +--------------------+
                        │
                        ▼
                 next()/send()
                        │
                 从 f_lasti 恢复执行
                        │
                   yield 再次暂停
                        │
                 更新 f_locals/f_lasti
                        │
                     return
                        │
                        ▼
                 gi_frame = None
```

## 黄金法则

> **Generator = Code Object + Persistent PyFrameObject + Instruction Pointer (`f_lasti`) + Local State (`f_locals`)。**

`yield` 的真正作用不是“返回一个值”，而是：

1. 返回当前值给调用者；
2. **冻结当前 `PyFrameObject`（保存整个执行现场）；**
3. 将当前字节码位置记录到 `f_lasti`；
4. 保留所有局部变量于 `f_locals`；
5. 等待下一次 `next()` 或 `send()`，从同一个 Frame 的 `f_lasti` 位置继续执行。

因此，Generator 可以被理解为：**一个拥有自己独立栈帧（Frame）的可恢复函数实例**，而不是普通函数的多次调用。