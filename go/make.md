# Golang `make` 完全总结：用法、原理与心智模式

> **一句话理解：**
>
> **Go 中 `make` 的作用不是"创建对象"，而是"初始化可工作的运行时数据结构（runtime object）"。**
>
> 它只用于三种引用类型：
>
> - slice
> - map
> - channel

---

# 一、为什么需要 make？

Go 有两类对象：

```
值类型(Value)

int
bool
struct
array
...

↓

直接就是数据
```

另一类：

```
引用类型(Reference-like)

slice
map
channel

↓

变量里面只是一个 descriptor（描述符）

真正的数据在 runtime
```

例如：

```
var s []int
```

此时：

```
+------+
| s    |
+------+
|ptr=nil
|len=0
|cap=0
+------+
```

真正的数据根本不存在。

所以：

```
append(s,1)
```

append 可以自动创建底层数组。

但是：

```
map

channel
```

不会自动创建。

因此：

```
make(...)
```

就是：

> **让 runtime 创建真正的数据结构，并把 descriptor 指向它。**

---

# 二、make 可以创建哪些类型？

Go 规定只能：

```
make([]T,...)

make(map[K]V,...)

make(chan T,...)
```

不能：

```
make(int)

make(struct{})

make(array)

make(string)
```

因为这些不是 runtime object。

---

# 三、心智模型（最重要）

## 普通变量

```
var x int
```

直接就是：

```
x
↓

10
```

变量就是数据。

---

## Slice

```
var s []int
```

实际上：

```
s

↓

+-------+
| ptr   |
| len   |
| cap   |
+-------+

↓

底层数组
```

make 的作用：

```
make([]int,5)

↓

创建底层数组

↓

填充 descriptor

↓

返回 descriptor
```

不是创建 slice。

而是：

> 创建 slice 背后的数组。

---

## Map

```
var m map[string]int
```

实际上：

```
m

↓

pointer

↓

runtime.hmap
```

如果：

```
m["a"]=1
```

因为：

```
pointer=nil
```

所以 panic：

```
assignment to entry in nil map
```

必须：

```
make(map[string]int)
```

runtime：

```
创建 hmap

↓

创建 bucket

↓

返回 map descriptor
```

---

## Channel

```
var ch chan int
```

实际上：

```
pointer=nil
```

make：

```
make(chan int)

↓

runtime 创建 hchan
```

里面包括：

```
buffer

send queue

recv queue

mutex

...
```

---

# 四、make 与 new 的区别

很多新人容易混。

## new

```
p := new(int)
```

等价：

```
var x int

return &x
```

得到：

```
*int
```

返回：

```
指针
```

---

## make

```
make(map)

make(slice)

make(chan)
```

返回：

```
map

slice

channel
```

不是指针。

虽然：

```
map

slice

channel
```

内部都含有指针。

所以：

```
make
```

返回的是：

```
descriptor
```

不是：

```
*descriptor
```

---

# 五、Slice 的 make

## 最简单

```go
s := make([]int, 5)
```

得到：

```
len = 5

cap = 5
```

内容：

```
0 0 0 0 0
```

---

## 指定 capacity

```go
s := make([]int, 5, 10)
```

表示：

```
len = 5

cap = 10
```

内存：

```
capacity

+--------------------------------+

0 0 0 0 0 ? ? ? ? ?

^^^^^^^^^

有效元素(len)
```

---

## append

```
append
```

直到：

```
len==cap
```

才扩容。

例如：

```
len=5

cap=10
```

append 五次：

```
不用扩容
```

第六次：

```
runtime grows
```

---

# 六、为什么要指定 capacity？

例如：

```go
s := make([]int,0,1000)
```

随后：

```go
for i:=0;i<1000;i++{
    s=append(s,i)
}
```

不会扩容。

如果：

```
cap=0
```

append：

```
0

↓

1

↓

2

↓

4

↓

8

↓

16

↓

...
```

不断 realloc。

所以：

```
提前知道大小

↓

提前分配

↓

减少 GC

↓

减少复制
```

这是 Go 最重要的优化之一。

---

# 七、Map 的 make

最普通：

```go
m:=make(map[string]int)
```

然后：

```go
m["a"]=1
```

---

## 指定 hint

```go
m:=make(map[string]int,1000)
```

注意：

```
1000

不是 bucket 数量
```

只是：

```
hint
```

runtime：

```
估算 bucket 数量
```

减少：

```
rehash
```

不是：

```
固定容量
```

map 没有：

```
cap()
```

---

# 八、为什么 nil map 可以读取？

例如：

```go
var m map[string]int

fmt.Println(m["a"])
```

输出：

```
0
```

因为：

```
lookup

↓

发现 nil

↓

返回 zero value
```

但是：

```
写
```

必须：

```
真正 bucket
```

否则：

```
panic
```

---

# 九、Channel 的 make

无缓冲：

```go
ch:=make(chan int)
```

内部：

```
buffer size = 0
```

发送：

```
必须等待接收者
```

---

有缓冲：

```go
ch:=make(chan int,100)
```

内部：

```
ring buffer

capacity=100
```

发送：

```
buffer 未满

↓

立即返回
```

---

# 十、make 的底层做了什么？

例如：

```go
make([]int,5,10)
```

runtime：

```
↓

runtime.makeslice()

↓

malloc()

↓

创建数组

↓

返回 SliceHeader
```

SliceHeader：

```go
type SliceHeader struct{
    Data uintptr
    Len int
    Cap int
}
```

用户得到：

```
SliceHeader
```

里面：

```
Data

↓

数组
```

---

Map：

```
make

↓

runtime.makemap()

↓

创建 hmap

↓

bucket

↓

overflow bucket

↓

返回 map descriptor
```

---

Channel：

```
make

↓

runtime.makechan()

↓

创建 hchan

↓

buffer

↓

lock

↓

sendq

↓

recvq
```

---

# 十一、make 与 nil 的区别

## Slice

```go
var s []int
```

```
nil
```

可以：

```go
append
```

因为：

```
append

↓

自动分配数组
```

所以：

```
nil slice

可以直接 append
```

---

## Map

```
nil
```

不能：

```
insert
```

必须：

```
make
```

---

## Channel

```
nil channel
```

发送：

```
永久阻塞
```

接收：

```
永久阻塞
```

所以：

```
必须 make
```

---

# 十二、什么时候使用 make？

| 类型 | 是否需要 make | 原因 |
|------|---------------|------|
| slice | 推荐 | 提前分配底层数组，减少扩容 |
| map | 必须（写入前） | 创建 hmap 与 bucket |
| channel | 必须 | 创建 hchan 与队列 |
| struct | 不需要 | 值类型直接初始化 |
| array | 不需要 | 固定大小值类型 |
| int/bool/string | 不需要 | 值类型 |

---

# 十三、`make` 的最佳实践

## 1. 已知元素数量时，预分配 slice 容量

```go
items := make([]Item, 0, 1000)

for _, v := range source {
    items = append(items, convert(v))
}
```

避免频繁扩容和数据复制。

---

## 2. 已知 map 大小时，提供容量提示

```go
m := make(map[string]User, 5000)
```

降低扩容与 rehash 成本。

---

## 3. 根据业务选择合适的 channel 缓冲区

```go
// 同步通信
syncCh := make(chan Task)

// 异步队列
queue := make(chan Task, 100)
```

不要盲目设置很大的缓冲区，应结合生产者/消费者速率。

---

## 4. 区分 nil slice 与空 slice

```go
var a []int          // nil slice
b := make([]int, 0)  // 空 slice
```

两者都可以 `append`，但在 JSON 编码等场景中可能表现不同（`nil` 常编码为 `null`，空 slice 常编码为 `[]`）。

---

# 十四、最终心智模型（80/20）

```
                make

                  │
                  │
      初始化 Runtime 数据结构
                  │
        ┌─────────┼─────────┐
        │         │         │
      slice      map      channel
        │         │         │
   底层数组      hmap      hchan
        │         │         │
   descriptor   bucket    buffer+queue
        │         │         │
        返回可用对象，而不是指针
```

---

# 一句话总结

> **`make` 不是“创建变量”，而是“初始化 Go Runtime 管理的引用类型”。变量本身只是一个描述符（descriptor），`make` 的真正工作是为 slice、map 和 channel 创建背后的运行时数据结构，并返回一个可以立即使用的描述符。**

---

# 延伸阅读：最佳实践与 10+ 示例

- [Slice 最佳实践与示例](slice-practices.md)
- [Map 最佳实践与示例](map-practices.md)
- [Channel 最佳实践与示例](chan-practices.md)