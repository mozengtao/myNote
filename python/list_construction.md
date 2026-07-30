# Python List Construction：构造 List 的完整心智模式

> **核心思想：**
>
> Python 中，**list 并不是只有一种创建方式，而是根据数据来源(Data Source)进行构造。**
>
> 可以把所有 list 的创建方式统一理解为：
>
> ```
>                 Data Source
>                      │
>      ┌───────────────┼────────────────┐
>      │               │                │
> Literal          Iterable        Existing List
>      │               │                │
>      └───────────────┼────────────────┘
>                      │
>             Build / Transform
>                      │
>                     List
> ```
>
> 因此，可以把 list construction 分成六大类：
>
> 1. Literal（直接写出来）
> 2. From Iterable（从可迭代对象构造）
> 3. Transform（映射、过滤、展开）
> 4. Merge（多个序列组合）
> 5. Generate（按规则生成）
> 6. Copy（已有 list 构造新 list）

---

# 一、List Literal（最直接）

## 示例1：空 list

```python
lst = []
```

等价于

```python
lst = list()
```

适合：

- 初始化
- 占位

---

## 示例2：普通 literal

```python
nums = [1, 2, 3]
```

最常见。

---

## 示例3：混合类型

```python
obj = [
    1,
    "hello",
    True,
    None,
    {"a": 1},
]
```

Python list 可以存放任何对象。

---

# 二、list() Constructor

list() 的本质：

> **把任何 Iterable 消费掉，生成一个 list。**

心智模型：

```
Iterable
    │
 list()
    │
 List
```

---

## 示例4：tuple → list

```python
t = (1, 2, 3)

lst = list(t)
```

结果

```
[1,2,3]
```

---

## 示例5：set → list

```python
s = {3, 1, 2}

lst = list(s)
```

注意：

set 无序。

---

## 示例6：string → list

```python
list("hello")
```

得到

```
['h','e','l','l','o']
```

每个字符都是一个元素。

---

## 示例7：dict → list

```python
d = {
    "a": 1,
    "b": 2,
}

list(d)
```

得到

```
['a','b']
```

因为 dict 默认迭代 key。

---

## 示例8：dict.values()

```python
list(d.values())
```

得到

```
[1,2]
```

---

## 示例9：dict.items()

```python
list(d.items())
```

得到

```
[
 ('a',1),
 ('b',2)
]
```

---

## 示例10：range()

```python
nums = list(range(5))
```

得到

```
[0,1,2,3,4]
```

这是最经典的数据生成方式。

---

# 三、List Comprehension（最重要）

Python 推荐方式。

心智模型：

```
Iterable
    │
for x
    │
transform
    │
append
    │
List
```

可以理解成：

```
new_list = []

for x in iterable:
    new_list.append(...)
```

---

## 示例11：简单映射

```python
nums = [1,2,3]

square = [
    x*x
    for x in nums
]
```

得到

```
[1,4,9]
```

---

## 示例12：过滤

```python
evens = [
    x
    for x in range(10)
    if x % 2 == 0
]
```

得到

```
[0,2,4,6,8]
```

---

## 示例13：映射 + 过滤

```python
[
    x*x
    for x in range(10)
    if x % 2
]
```

---

## 示例14：双层循环

```python
pairs = [
    (x, y)
    for x in range(3)
    for y in range(2)
]
```

结果

```
[(0,0),
 (0,1),
 (1,0),
 ...]
```

---

## 示例15：展开二维数组

```python
matrix = [
    [1,2],
    [3,4]
]

flat = [
    item
    for row in matrix
    for item in row
]
```

得到

```
[1,2,3,4]
```

经典 flatten。

---

# 四、通过 append()

适用于：

不知道最终元素数量。

```python
result = []

for line in lines:
    result.append(process(line))
```

心智模型：

```
Start Empty

↓

append()

↓

append()

↓

append()

↓

Finished
```

---

## 示例16：动态构造

```python
result = []

for i in range(5):
    result.append(i*i)
```

---

# 五、extend()

extend 的本质：

> **把 iterable 每个元素追加进去。**

不是加入整个对象。

---

## 示例17

```python
a = [1,2]

a.extend([3,4])
```

得到

```
[1,2,3,4]
```

而不是

```
[1,2,[3,4]]
```

---

## 示例18：extend(range())

```python
lst = []

lst.extend(range(5))
```

得到

```
[0,1,2,3,4]
```

---

## 示例19：extend(string)

```python
letters = []

letters.extend("abc")
```

得到

```
['a','b','c']
```

因为 string 也是 iterable。

---

# 六、+ 合并

```python
a = [1,2]
b = [3,4]

c = a + b
```

得到

```
[1,2,3,4]
```

特点：

产生新 list。

---

# 七、*= 重复构造

## 示例20

```python
zeros = [0] * 5
```

得到

```
[0,0,0,0,0]
```

非常常见。

---

## 示例21：二维列表陷阱

错误：

```python
matrix = [[0]*3]*4
```

所有行引用同一个对象。

正确：

```python
matrix = [
    [0]*3
    for _ in range(4)
]
```

---

# 八、Iterable Unpacking（Python 3）

## 示例22

```python
a = [1,2]
b = [3,4]

c = [*a, *b]
```

得到

```
[1,2,3,4]
```

---

## 示例23：混合

```python
nums = [
    0,
    *range(5),
    100
]
```

得到

```
[0,0,1,2,3,4,100]
```

---

## 示例24：多个 iterable

```python
result = [
    *"abc",
    *range(3)
]
```

得到

```
['a','b','c',0,1,2]
```

---

# 九、list(generator)

generator 可以一次生成。

## 示例25

```python
g = (
    x*x
    for x in range(5)
)

lst = list(g)
```

得到

```
[0,1,4,9,16]
```

generator 会被消费。

---

# 十、map()

## 示例26

```python
lst = list(
    map(
        str.upper,
        ["a","b","c"]
    )
)
```

得到

```
["A","B","C"]
```

---

# 十一、filter()

## 示例27

```python
lst = list(
    filter(
        lambda x: x>0,
        [-1,0,1,2]
    )
)
```

得到

```
[1,2]
```

---

# 十二、zip()

## 示例28

```python
a = [1,2]
b = ["A","B"]

list(zip(a,b))
```

得到

```
[
 (1,'A'),
 (2,'B')
]
```

---

# 十三、enumerate()

## 示例29

```python
list(
    enumerate(
        ["a","b","c"]
    )
)
```

得到

```
[
 (0,'a'),
 (1,'b'),
 (2,'c')
]
```

---

# 十四、sorted()

sorted 返回新的 list。

## 示例30

```python
nums = [5,3,1]

sorted(nums)
```

得到

```
[1,3,5]
```

---

# 十五、Copy Construction

## 示例31：切片复制

```python
copy = original[:]
```

---

## 示例32：list()

```python
copy = list(original)
```

---

## 示例33：copy()

```python
copy = original.copy()
```

三者都是浅拷贝。

---

# 十六、构造二维结构

## 示例34

```python
table = [
    [0 for _ in range(5)]
    for _ in range(3)
]
```

推荐方式。

---

# 十七、读取文件

## 示例35

```python
with open("test.txt") as f:
    lines = list(f)
```

文件对象也是 iterable。

---

# 十八、split()

## 示例36

```python
line = "a,b,c"

lst = line.split(",")
```

得到

```
["a","b","c"]
```

这是文本处理中最常见的 list 构造方式之一。

---

# List Construction 总结（按数据来源）

| 数据来源 | 常用方法 | 是否创建新 List | 典型场景 |
|-----------|----------|----------------|----------|
| Literal | `[]` | ✓ | 已知元素 |
| Iterable | `list(iterable)` | ✓ | tuple、set、dict、range、generator |
| Mapping | List Comprehension | ✓ | 转换元素 |
| Filtering | List Comprehension / `filter()` | ✓ | 条件筛选 |
| Dynamic | `append()` | 原地修改 | 不确定数量、逐步构造 |
| Batch Append | `extend()` | 原地修改 | 合并 iterable |
| Merge | `+` / `[*a, *b]` | ✓ | 拼接多个序列 |
| Repeat | `*` | ✓ | 初始化固定值 |
| Generator | `list(generator)` | ✓ | 延迟计算后收集结果 |
| Functional | `map()` / `filter()` | ✓ | 函数式转换 |
| Pairing | `zip()` | ✓ | 多序列配对 |
| Indexing | `enumerate()` | ✓ | 生成 `(index, value)` |
| Sorting | `sorted()` | ✓ | 排序得到新列表 |
| Copy | `[:]`、`list()`、`copy()` | ✓ | 浅拷贝 |
| File/Text | `list(file)`、`split()` | ✓ | 文件、CLI、日志解析 |

---

# List Construction 的统一心智模型（Golden Rule）

所有列表构造方式都可以统一为一个数据流过程：

```text
                Data Source
                     │
     ┌───────────────┼──────────────────────┐
     │               │                      │
 Literal         Iterable              Existing List
     │               │                      │
     └───────────────┼──────────────────────┘
                     │
              Build / Transform
                     │
    ┌────────────────┼──────────────────────────┐
    │                │                          │
 append()      extend()                comprehension
 map()         filter()                unpacking(*)
 zip()         enumerate()             sorted()
 split()       list()                  copy()
                     │
                     ▼
                 New List
```

**黄金法则：**

- **已知元素** → 使用 **List Literal (`[]`)**
- **已有可迭代对象** → 使用 **`list(iterable)`**
- **需要转换或过滤** → 优先使用 **List Comprehension**
- **逐步累积元素** → 使用 **`append()`**
- **批量追加另一个可迭代对象** → 使用 **`extend()`**
- **组合多个序列** → 使用 **`+`** 或 **Iterable Unpacking (`*`)**
- **初始化固定长度** → 使用 **`*`**（注意嵌套列表共享引用陷阱）
- **复制已有列表** → 使用 **`copy()`**、切片 `[:]` 或 **`list()`**
- **从文件、文本或其他数据源读取** → 将其视为 **Iterable**，再转换或推导构造。