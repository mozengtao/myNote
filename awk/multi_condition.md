# AWK 组合多个条件（Multiple Conditions）心智模型

## 核心思想

AWK 的执行模型可以理解为：

```
对于输入的每一行
        │
        ▼
   判断 Pattern（条件）
        │
        ├── true ──► 执行 Action
        │
        └── false ─► 跳过
```

Pattern 本质就是一个 **Boolean Expression（布尔表达式）**。

因此，AWK 的多个条件组合，本质就是：

> 利用布尔表达式（Boolean Expression）组合多个判断条件。

---

# 一、AWK 条件类型

AWK 的 Pattern 可以由下面几类组成：

```
Pattern
│
├── 正则表达式
├── 比较表达式
├── 数值判断
├── 字符串判断
├── 内置变量判断
└── 多个条件组合
```

例如

```
$1 ~ /^vmc/
$2 == "UP"
$3 > 100
NF >= 3
!/sim/
```

都属于 Pattern。

---

# 二、逻辑运算符

AWK 支持和 C 语言一致的逻辑运算：

| 运算符 | 含义 | 示例 |
|---------|------|------|
| && | AND | A && B |
| \|\| | OR | A \|\| B |
| ! | NOT | !A |
| () | 分组 | (A && B) \|\| C |

例如：

```
A && B && C
```

表示：

```
全部满足
```

而

```
A || B
```

表示

```
满足一个即可
```

---

# 三、多个条件的心智模型

不要把它理解成 awk 特有语法。

实际上就是：

```
if (
    条件1 &&
    条件2 &&
    条件3
)
{
    执行动作
}
```

AWK 只是把 if 省略了：

```
condition1 &&
condition2 &&
condition3
{
    action
}
```

---

# 四、常见组合模式

---

## 模式1：AND（全部满足）

例如：

```
vmc001 online
vmc002 sim
sim001 online
vmc003 active
```

希望：

- 第一列以 vmc 开头
- 第一列不能包含 sim

```
awk '
$1 ~ /^vmc/ &&
$1 !~ /sim/
{
    print $2
}
'
```

输出：

```
online
active
```

ASCII：

```
          是 vmc ?
              │
      ┌───────┴────────┐
      │                │
     否               是
      │                │
   跳过         包含 sim ?
                    │
          ┌─────────┴─────────┐
          │                   │
         是                  否
          │                   │
        跳过             print $2
```

---

## 模式2：OR（满足一个即可）

例如：

```
vmc001 online
server001 active
switch001 down
```

希望：

- 第一列是 vmc
- 或者 server

```
awk '
$1 ~ /^vmc/ ||
$1 ~ /^server/
{
    print
}
'
```

输出：

```
vmc001 online
server001 active
```

---

## 模式3：NOT

例如：

```
vmc001 online
vmc002 sim
vmc003 active
```

希望：

不要 sim。

```
awk '
!/sim/
{
    print
}
'
```

输出：

```
vmc001 online
vmc003 active
```

---

## 模式4：AND + NOT

例如：

```
vmc001 online
vmc002 sim
vmc003 active
server001 online
```

希望：

- vmc
- 不包含 sim

```
awk '
/^vmc/ &&
!/sim/
{
    print
}
'
```

输出：

```
vmc001 online
vmc003 active
```

---

## 模式5：字段比较

例如：

```
vmc001 UP
vmc002 DOWN
vmc003 UP
```

只输出 UP。

```
awk '
$2=="UP"
{
    print $1
}
'
```

输出：

```
vmc001
vmc003
```

---

## 模式6：多个字段判断

例如：

```
vmc001 UP 100
vmc002 UP 20
vmc003 DOWN 300
vmc004 UP 500
```

希望：

- 第二列 UP
- 第三列 >100

```
awk '
$2=="UP" &&
$3>100
{
    print
}
'
```

输出：

```
vmc004 UP 500
```

---

## 模式7：正则 + 字段比较

例如：

```
vmc001 UP
vmc002 DOWN
sim001 UP
```

希望：

- vmc
- UP

```
awk '
$1~/^vmc/ &&
$2=="UP"
{
    print
}
'
```

输出：

```
vmc001 UP
```

---

## 模式8：字段数量判断

例如：

```
vmc001 UP
vmc002
vmc003 DOWN
```

要求至少两列。

```
awk '
NF>=2
{
    print
}
'
```

输出：

```
vmc001 UP
vmc003 DOWN
```

---

## 模式9：字符串函数组合

例如：

```
vmc001
vmcsim001
vmc888
```

希望：

- 前缀 vmc
- 不包含 sim

```
awk '
index($1,"vmc")==1 &&
index($1,"sim")==0
{
    print
}
'
```

输出：

```
vmc001
vmc888
```

---

## 模式10：复杂组合

例如：

```
vmc001 UP
vmc002 DOWN
server001 UP
server002 DOWN
switch001 UP
```

希望：

```
(vmc 或 server)
并且
状态为 UP
```

```
awk '
(
    $1~/^vmc/ ||
    $1~/^server/
)
&&
$2=="UP"
{
    print
}
'
```

输出：

```
vmc001 UP
server001 UP
```

---

# 五、Pattern 与 Action 的关系

```
Pattern
{
    Action
}
```

等价于

```
if (Pattern)
{
    Action
}
```

例如：

```
$3>100
{
    print
}
```

实际上就是：

```
if ($3>100)
{
    print
}
```

---

# 六、推荐的编写风格

简单条件：

```
awk '$2=="UP" {print}'
```

多个条件：

```
awk '
$1~/^vmc/ &&
$2=="UP" &&
$3>100
{
    print
}
'
```

复杂条件：

```
awk '
(
    $1~/^vmc/ ||
    $1~/^server/
)
&&
$2=="UP"
&&
$3>100
{
    print
}
'
```

这种格式具有：

- 条件清晰
- 易于增加或删除条件
- 类似高级语言中的 `if (...)`

---

# 七、常用条件组合速查

| 模式 | 示例 |
|------|------|
| AND | `$1~/^vmc/ && $2=="UP"` |
| OR | `$1~/^vmc/ \|\| $1~/^server/` |
| NOT | `!/sim/` |
| 正则 | `$1~/^vmc/` |
| 正则取反 | `$1!~/sim/` |
| 字符串相等 | `$2=="UP"` |
| 字符串不等 | `$2!="DOWN"` |
| 数值比较 | `$3>100` |
| 数值区间 | `$3>=10 && $3<=20` |
| 字段数量 | `NF>=3` |
| 空字段 | `$2==""` |
| 非空字段 | `$2!=""` |
| 字符串包含 | `index($1,"vmc")>0` |
| 字符串前缀 | `index($1,"vmc")==1` |
| 长度判断 | `length($1)>5` |

---

# 八、最佳实践与心智模式

1. **把 Pattern 看成布尔表达式**：它就是 `if (condition)` 的条件。
2. **优先针对字段判断**：使用 `$1`、`$2` 等字段，而不是直接匹配整行，可减少误匹配。
3. **使用 `&&`、`||`、`!` 组合复杂逻辑**：与 C、Python 等语言一致，易于理解和维护。
4. **复杂条件使用括号分组**：提高可读性，避免运算优先级带来的歧义。
5. **多行格式优于长单行**：当条件较多时，每个条件独占一行，更容易扩展和调试。