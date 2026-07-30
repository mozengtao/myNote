# Python Dict Construction：构造 Dict 的完整心智模式

> **核心思想：**
>
> Python 中，**dict 并不是只有一种创建方式，而是根据数据来源(Data Source)进行构造。**
>
> 可以把所有 dict 的创建方式统一理解为：
>
> ```
>                       Data Source
>                            │
>      ┌───────────┬─────────┼─────────┬───────────┐
>      │           │         │         │           │
> Literal   Pairs(Iterable) Keys+Values Existing Dict  Comprehension
>      │           │         │         │           │
>      └───────────┴─────────┼─────────┴───────────┘
>                            │
>                   Build / Transform / Merge
>                            │
>                          Dict
> ```
>
> 因此，可以把 dict construction 分成八大类：
>
> 1. Literal（直接写出来）
> 2. From Pairs / Iterable（从键值对序列构造）
> 3. Comprehension（映射、过滤、反转）
> 4. Dynamic（逐步赋值）
> 5. Merge（多个 dict 合并）
> 6. Default / Count（defaultdict、Counter）
> 7. Copy（已有 dict 构造新 dict）
> 8. Nested（构造嵌套结构）

---

# 一、Dict Literal（最直接）

## 示例1：空 dict

```python
d = {}
```

等价于

```python
d = dict()
```

适合：

- 初始化
- 占位

---

## 示例2：普通 literal

```python
person = {
    "name": "Tom",
    "age": 18,
}
```

最常见。

---

## 示例3：混合类型 value

```python
obj = {
    "id": 1,
    "tags": ["a", "b"],
    "active": True,
    "meta": None,
    "nested": {"x": 1},
}
```

dict 的 value 可以是任何对象；key 必须是可哈希(hashable)对象。

---

# 二、dict() Constructor

dict() 的本质：

> **把 "键值对序列" 或 "关键字参数" 消费掉，生成一个 dict。**

心智模型：

```
Pairs / kwargs
      │
   dict()
      │
    Dict
```

---

## 示例4：zip(keys, values) → dict

```python
keys = ["a", "b", "c"]
values = [1, 2, 3]

d = dict(zip(keys, values))
```

结果

```
{'a': 1, 'b': 2, 'c': 3}
```

最经典的"两个平行序列合并成 dict"写法。

---

## 示例5：list of tuples → dict

```python
pairs = [
    ("a", 1),
    ("b", 2),
]

d = dict(pairs)
```

结果

```
{'a': 1, 'b': 2}
```

---

## 示例6：list of lists → dict

```python
pairs = [
    ["a", 1],
    ["b", 2],
]

d = dict(pairs)
```

结果同上。只要每个元素是"长度为 2 的 iterable"即可。

---

## 示例7：keyword 参数构造

```python
d = dict(a=1, b=2, c=3)
```

结果

```
{'a': 1, 'b': 2, 'c': 3}
```

限制：key 必须是合法的标识符字符串，不能是变量、数字等。

---

## 示例8：dict.fromkeys()

```python
keys = ["a", "b", "c"]

d = dict.fromkeys(keys, 0)
```

结果

```
{'a': 0, 'b': 0, 'c': 0}
```

注意：

```python
d = dict.fromkeys(keys, [])
```

所有 key 会指向**同一个** list 对象，是常见陷阱，类似 `[[0]*3]*4` 的坑。

---

# 三、Dict Comprehension（最重要）

Python 推荐方式。

心智模型：

```
Iterable
    │
 for k, v
    │
 transform
    │
  d[k] = v
    │
   Dict
```

可以理解成：

```
new_dict = {}

for k, v in pairs:
    new_dict[key_expr] = value_expr
```

---

## 示例9：简单映射

```python
nums = [1, 2, 3]

squares = {
    x: x*x
    for x in nums
}
```

得到

```
{1: 1, 2: 4, 3: 9}
```

---

## 示例10：过滤

```python
evens = {
    x: x*x
    for x in range(10)
    if x % 2 == 0
}
```

得到

```
{0: 0, 2: 4, 4: 16, 6: 36, 8: 64}
```

---

## 示例11：从两个平行序列构造（comprehension 版）

```python
keys = ["a", "b", "c"]
values = [1, 2, 3]

d = {
    k: v
    for k, v in zip(keys, values)
}
```

结果

```
{'a': 1, 'b': 2, 'c': 3}
```

---

## 示例12：反转 key/value

```python
d = {"a": 1, "b": 2}

reversed_d = {
    v: k
    for k, v in d.items()
}
```

得到

```
{1: 'a', 2: 'b'}
```

注意：如果 value 有重复，反转后会丢失数据（后面的会覆盖前面的）。

---

## 示例13：双层循环

```python
matrix_dict = {
    (x, y): x*y
    for x in range(2)
    for y in range(3)
}
```

结果

```
{
 (0,0):0, (0,1):0, (0,2):0,
 (1,0):0, (1,1):1, (1,2):2
}
```

---

# 四、逐步构造：动态赋值

适用于：

不知道最终 key 的数量，或者需要根据条件逐步填充。

```python
result = {}

for line in lines:
    k, v = parse(line)
    result[k] = v
```

心智模型：

```
Start Empty

↓

d[k1] = v1

↓

d[k2] = v2

↓

Finished
```

---

## 示例14：动态构造

```python
counts = {}

for word in ["a", "b", "a", "c"]:
    counts[word] = counts.get(word, 0) + 1
```

得到

```
{'a': 2, 'b': 1, 'c': 1}
```

`get(key, default)` 是动态构造 dict 时最常用的安全取值方式。

---

# 五、update() 合并

update 的本质：

> **把另一个 dict 或键值对序列的内容合并进来，原地修改，重复 key 会被覆盖。**

---

## 示例15：dict 合并 dict

```python
a = {"x": 1, "y": 2}
b = {"y": 20, "z": 3}

a.update(b)
```

得到

```
{'x': 1, 'y': 20, 'z': 3}
```

`a` 被原地修改，`b` 的 `y` 覆盖了 `a` 的 `y`。

---

## 示例16：update(键值对序列)

```python
d = {}

d.update([("a", 1), ("b", 2)])
```

得到

```
{'a': 1, 'b': 2}
```

---

# 六、合并运算符（生成新 Dict，不修改原对象）

## 示例17：`|` 合并（Python 3.9+）

```python
a = {"x": 1, "y": 2}
b = {"y": 20, "z": 3}

c = a | b
```

得到

```
{'x': 1, 'y': 20, 'z': 3}
```

特点：

产生**新** dict，`a`、`b` 都不变。

---

## 示例18：`|=` 就地合并

```python
a |= b
```

等价于

```python
a = a | b
```

但语义上是就地更新，效果类似 `update()`。

---

## 示例19：`**` 展开合并（旧式写法，兼容性最好）

```python
c = {**a, **b}
```

得到

```
{'x': 1, 'y': 20, 'z': 3}
```

在字典字面量里展开另一个 dict 的内容，等价于 `|`，但兼容 Python 3.5+。

---

## 示例20：`**` 混合 literal 与展开

```python
d = {
    "default": True,
    **a,
    "extra": 100,
}
```

得到

```
{'default': True, 'x': 1, 'y': 2, 'extra': 100}
```

---

# 七、setdefault() 与 defaultdict

## 示例21：setdefault()

```python
d = {}

d.setdefault("a", []).append(1)
d.setdefault("a", []).append(2)
```

得到

```
{'a': [1, 2]}
```

如果 key 不存在则设置默认值并返回；如果已存在则直接返回现有值。

---

## 示例22：defaultdict 构造（分组场景）

```python
from collections import defaultdict

groups = defaultdict(list)

for name, dept in [("Tom", "IT"), ("Jerry", "IT"), ("Ann", "HR")]:
    groups[dept].append(name)
```

得到

```
{'IT': ['Tom', 'Jerry'], 'HR': ['Ann']}
```

`defaultdict(list)` 免去了每次判断 key 是否存在的麻烦，是分组/聚合场景的标准写法。

---

## 示例23：转回普通 dict

```python
normal = dict(groups)
```

`defaultdict` 本身是 dict 的子类，随时可以用 `dict()` 转换回普通 dict。

---

# 八、Counter() 构造（计数场景）

## 示例24：从 iterable 计数

```python
from collections import Counter

c = Counter("abracadabra")
```

得到

```
Counter({'a': 5, 'b': 2, 'r': 2, 'c': 1, 'd': 1})
```

`Counter` 也是 dict 的子类，专门用于计数场景，比手写 `d[k] = d.get(k, 0) + 1` 更简洁。

---

## 示例25：从 list 计数

```python
words = ["a", "b", "a", "c", "b", "a"]

c = Counter(words)
```

得到

```
Counter({'a': 3, 'b': 2, 'c': 1})
```

---

# 九、从 items()/keys()/values() 重新构造

## 示例26：筛选部分 key

```python
d = {"a": 1, "b": 2, "c": 3}

filtered = {
    k: v
    for k, v in d.items()
    if v > 1
}
```

得到

```
{'b': 2, 'c': 3}
```

---

## 示例27：只保留 key，重置 value

```python
reset = dict.fromkeys(d.keys(), 0)
```

得到

```
{'a': 0, 'b': 0, 'c': 0}
```

---

# 十、Copy Construction

## 示例28：dict()

```python
copy = dict(original)
```

---

## 示例29：copy()

```python
copy = original.copy()
```

---

## 示例30：comprehension 复制

```python
copy = {
    k: v
    for k, v in original.items()
}
```

三者都是**浅拷贝**：嵌套的可变对象（如 value 是 list）仍然共享引用。

---

# 十一、构造嵌套结构

## 示例31：二维 dict（矩阵型）

```python
table = {
    (i, j): 0
    for i in range(3)
    for j in range(3)
}
```

推荐方式，key 用 tuple 表示坐标，天然可哈希。

---

## 示例32：dict of dict（陷阱与正确写法）

错误：

```python
row = {"a": 0}
matrix = {i: row for i in range(3)}
```

所有 key 共享同一个 `row` 对象引用。

正确：

```python
matrix = {
    i: {"a": 0}
    for i in range(3)
}
```

每次循环都新建一个字典，与 list 的 `[[0]*3]*4` 陷阱同源。

---

# Dict Construction 总结（按数据来源）

| 数据来源 | 常用方法 | 是否创建新 Dict | 典型场景 |
|-----------|----------|----------------|----------|
| Literal | `{}` / `{k: v}` | ✓ | 已知键值对 |
| Parallel Sequences | `dict(zip(keys, values))` | ✓ | 两个平行序列合并 |
| Pairs | `dict(pairs)` | ✓ | list/tuple of (k, v) |
| Keywords | `dict(a=1, b=2)` | ✓ | key 是合法标识符 |
| Fixed Value | `dict.fromkeys(keys, v)` | ✓ | 批量初始化相同默认值 |
| Mapping/Filtering | Dict Comprehension | ✓ | 转换、过滤、反转 key/value |
| Dynamic | `d[k] = v` | 原地修改 | 不确定数量、逐步构造 |
| Batch Merge | `update()` | 原地修改 | 合并另一个 dict/pairs |
| Merge (new) | `\|` / `{**a, **b}` | ✓ | 拼接多个 dict，不改原对象 |
| In-place Merge | `\|=` | 原地修改 | 就地合并 |
| Default | `setdefault()` / `defaultdict()` | 原地修改 | 分组、聚合、避免 KeyError |
| Counting | `Counter()` | ✓ | 元素计数 |
| Copy | `dict()`、`copy()`、comprehension | ✓ | 浅拷贝 |
| Nested | 双层 comprehension | ✓ | 矩阵、多维索引 |

---

# Dict Construction 的统一心智模型（Golden Rule）

所有字典构造方式都可以统一为一个数据流过程：

```text
                      Data Source
                           │
      ┌───────────┬─────────┼─────────┬───────────┐
      │           │         │         │           │
  Literal      Pairs    Keys+Values Existing Dict  Comprehension
      │           │         │         │           │
      └───────────┴─────────┼─────────┴───────────┘
                           │
                  Build / Transform / Merge
                           │
    ┌──────────────────────┼──────────────────────────┐
    │                      │                          │
d[k]=v               update()                  comprehension
setdefault()          |  /  |=                 fromkeys()
defaultdict()         {**a, **b}                Counter()
                           │
                           ▼
                        New Dict
```

**黄金法则：**

- **已知键值对，数量固定** → 使用 **Dict Literal (`{}`)**
- **有两个平行序列（keys、values）** → 使用 **`dict(zip(keys, values))`**
- **有键值对序列（如数据库查询结果）** → 使用 **`dict(pairs)`**
- **key 是合法标识符且数量少** → 使用 **`dict(a=1, b=2)`**
- **需要给一批 key 设置相同的初始值** → 使用 **`dict.fromkeys()`**（注意可变默认值共享引用陷阱）
- **需要转换、过滤或反转 key/value** → 优先使用 **Dict Comprehension**
- **逐步累积键值对** → 使用 **`d[k] = v`** 或 **`get(k, default)`**
- **合并另一个 dict，允许覆盖原对象** → 使用 **`update()`** 或 **`|=`**
- **合并多个 dict，但不想修改原对象** → 使用 **`|`** 或 **`{**a, **b}`**
- **按 key 分组、聚合，避免 KeyError** → 使用 **`setdefault()`** 或 **`defaultdict()`**
- **对元素计数** → 使用 **`Counter()`**
- **复制已有 dict** → 使用 **`dict()`**、**`copy()`** 或 comprehension（均为浅拷贝）
- **构造嵌套/多维结构** → 使用**双层 comprehension**，避免在循环外创建被共享的可变对象
